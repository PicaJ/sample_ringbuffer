#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <unistd.h>

struct ringbuffer
{
    uint8_t *buffer_ptr;

    uint16_t read_mirror : 1;
    uint16_t read_index : 15;

    uint16_t write_mirror : 1;
    uint16_t write_index : 15;

    int16_t buffer_size;
    pthread_mutex_t mutex_lock;
};

enum ringbuffer_state
{
    RINGBUFFER_EMPTY,
    RINGBUFFER_FULL,
    RINGBUFFER_HALFFULL,
    RINGBUFFER_INVALID,
};

struct ringbuffer* ringbuffer_create(int16_t length);
void ringbuffer_destroy(struct ringbuffer *rb);

static inline int16_t ringbuffer_get_size(struct ringbuffer *rb)
{
   return rb->buffer_size;    
}

#define ringbuffer_space_len(rb) ((rb)->buffer_size - ringbuffer_data_len(rb))

//======================

void ringbuffer_init(struct ringbuffer *rb, uint8_t *pool, int size)
{
    if(rb == NULL)
    {
        return;
    }
 
    rb->read_mirror = rb->read_index = 0;
    rb->write_mirror = rb->write_index = 0;
 
    rb->buffer_ptr = pool;
    rb->buffer_size = size;
    pthread_mutex_init(&rb->mutex_lock,NULL);
 
    return;
}
 
 
void ringbuffer_reset(struct ringbuffer *rb)
{
    if(rb == NULL)
    {
        return;
    }
    
    rb->read_mirror = 0;
    rb->read_index = 0;
    rb->write_mirror = 0;
    rb->write_index = 0;
 
    return;
}
 
 
int ringbuffer_put(struct ringbuffer *rb, const uint8_t *ptr, int length)
{
    int size; 
 
    if(rb == NULL || length == 0)
    {
        return 0;
    }
    
    pthread_mutex_lock(&rb->mutex_lock);
    size = ringbuffer_space_len(rb);
    pthread_mutex_unlock(&rb->mutex_lock);
 
    if(size == 0)
        return 0;
 
    if (size < length)
    {
        length = size;
    }
 
    if (rb->buffer_size - rb->write_index > length)
    {
        memcpy(&rb->buffer_ptr[rb->write_index], ptr, length);
        rb->write_index += length; 
        return length;
    }
 
    memcpy(&rb->buffer_ptr[rb->write_index],&ptr[0],rb->buffer_size - rb->write_index);
    memcpy(&rb->buffer_ptr[0],&ptr[rb->buffer_size - rb->write_index],length - (rb->buffer_size - rb->write_index));
 
    pthread_mutex_lock(&rb->mutex_lock);
    rb->write_mirror = ~rb->write_mirror;
    rb->write_index = length - (rb->buffer_size - rb->write_index);
    pthread_mutex_unlock(&rb->mutex_lock);
 
    return length;
}
 
int ringbuffer_get(struct ringbuffer *rb, uint8_t *ptr, int length)
{
    if(rb == NULL || length == 0)
    {
        return 0;
    }
    
    int size; 
    
    pthread_mutex_lock(&rb->mutex_lock);
    size = ringbuffer_data_len(rb);
    pthread_mutex_unlock(&rb->mutex_lock);
 
    if (size == 0) return 0;
 
    if (size < length)
    {
        length = size;
    }
 
    if (rb->buffer_size - rb->read_index > length)
    {
        memcpy(ptr, &rb->buffer_ptr[rb->read_index], length);
        rb->read_index += length;
        return length;
    }
 
    memcpy(&ptr[0],&rb->buffer_ptr[rb->read_index],rb->buffer_size - rb->read_index);
    memcpy(&ptr[rb->buffer_size - rb->read_index], &rb->buffer_ptr[0], length - (rb->buffer_size - rb->read_index));
 
    pthread_mutex_lock(&rb->mutex_lock);
    rb->read_mirror = ~rb->read_mirror; 
    rb->read_index = length - (rb->buffer_size - rb->read_index);
    pthread_mutex_unlock(&rb->mutex_lock);
 
    return length;
}
 
enum ringbuffer_state ringbuffer_status(struct ringbuffer *rb) 
{
    if (rb->read_index == rb->write_index)
    {
        if (rb->read_mirror == rb->write_mirror)
        {
            return RINGBUFFER_EMPTY;
        }
        else
        {
            return RINGBUFFER_FULL;
        }
    }
 
    return RINGBUFFER_HALFFULL;
}
 
int ringbuffer_data_len(struct ringbuffer *rb)
{
    switch (ringbuffer_status(rb)) 
    {
        case RINGBUFFER_EMPTY:
            return 0;
        case RINGBUFFER_FULL:
            return rb->buffer_size; 
        case RINGBUFFER_HALFFULL:
        default:
            if (rb->write_index > rb->read_index)
                return rb->write_index - rb->read_index;
            else
                return rb->buffer_size - (rb->read_index - rb->write_index); 
    }
}





//==========================
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

struct ringbuffer *rb;
void *writer_thread(void *args)
{
    uint8_t w = 0;
    while(1)
    {
        /*pthread_mutex_lock(&mutex);*/
        if(ringbuffer_put(rb, &w, 1) == 1)
        w ++;
        /*pthread_mutex_unlock(&mutex);*/
    }

    return NULL;
}

void *reader_thread(void *args)
{
    uint8_t r;
    while(1)
    {
        /*pthread_mutex_lock(&mutex);*/
        if (ringbuffer_get(rb, &r, 1) == 1)
            printf("---%x---\n", r);
        /*pthread_mutex_unlock(&mutex);*/
    }

    return NULL;
}

int main(void)
{
    unsigned int rc;

#if 1
    uint8_t *buffer = (uint8_t *)malloc(1024 + sizeof(struct ringbuffer) + 16);

    if(buffer == NULL) return -1;
    if((unsigned long)buffer & 7 != 0)
    {
        printf("%s line %d not aligned.\n", __func__, __LINE__);
    return -1;
    }
   
    rb=(struct ringbuffer *)buffer;

    if(rb == NULL) return -1;
    ringbuffer_init(rb, buffer + sizeof(struct ringbuffer), 1024);
    printf("buffer = %p, sizeof(rb) = %ld.\n", buffer, sizeof(struct ringbuffer));

#else
    static struct ringbuffer rbb;

    uint8_t *buffer = (uint8_t *)malloc(1024);
    if(buffer == NULL) return -1;
    rb = &rbb;
    memset(rb, 0x00,sizeof(struct ringbuffer));
    memset(buffer, 0x00,1024);
    ringbuffer_init(rb, buffer, 1024);
#endif

    pthread_t writer;
    rc = pthread_create(&writer, NULL, writer_thread, NULL);
    if (rc)
    {
        printf("ERROR; return code is %d\n", rc);
        return EXIT_FAILURE;
    }

    pthread_t reader;
    rc = pthread_create(&reader, NULL, reader_thread, NULL);
    if (rc)
    {
        printf("ERROR; return code is %d\n", rc);
        return EXIT_FAILURE;
    }

    pthread_join(reader, NULL);
    pthread_join(writer, NULL);
    return EXIT_SUCCESS;
}
