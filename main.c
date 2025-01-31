#include "queue.h"

char m1 = 'a', m2 = 'b', m3 = 'c', m4 = 'd', m5 = 'e', m6 = 'f', m7 = 'g', m8 = 'h'; 

void* writerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    int size = 2;
    addMsg(queue, &m1);
    addMsg(queue, &m2);
    addMsg(queue, &m3);
    removeMsg(queue, &m2);
    addMsg(queue, &m4);
    setSize(queue, size);

    return NULL;
}

void* T1(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, self);

    return NULL;
}

void* T2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, self);
    getMsg(queue, self);
    getMsg(queue, self);

    return NULL;
}

void* T3(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, self);
    getMsg(queue, self);

    return NULL;
}

int main() 
{
    // Initialize a queue with a maximum size
    int maxSize = 3;
    TQueue* queue;
    queue = createQueue(maxSize);

    // Set up threads (4 subscribers and a writer)
    pthread_t writer, subscriber1, subscriber2, subscriber3;
    pthread_create(&subscriber1, NULL, T1, queue);
    pthread_create(&subscriber2, NULL, T2, queue);
    pthread_create(&subscriber3, NULL, T3, queue);
    pthread_create(&writer, NULL, writerFunc2, queue);

    pthread_join(subscriber1, NULL);
    pthread_join(subscriber2, NULL);
    pthread_join(subscriber3, NULL);
    pthread_join(writer, NULL);

    // Destroy the queue
    destroyQueue(queue);

    return 0;
}
