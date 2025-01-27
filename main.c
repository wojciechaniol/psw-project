#include "queue.h"

void* writerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    int* i = 0, j = 0;
    int newSize = -2;

    while(1)
    {
        addMsg(queue, i++);
		printf("%d\n", j);
        //setSize(queue, (newSize+queue->maxSize));
        addMsg(queue, i++);
		printf("%d\n", j);
        j++;
		printf("moving on\n");
        if (j >= 15)
        {
            destroyQueue(queue);
            return NULL;
        }
    }

    return NULL;
}

void* readerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, &self);

    while(queue != NULL)
    {
		printf("subscribed for thread: %lu\n", self);
        getMsg(queue, &self);
		printf("message recieved for thread: %lu\n",  self);
    }

    return NULL;
}

int main() 
{
    // Initialize a queue with a maximum size
    int maxSize = 5;
    TQueue* queue;
    queue = createQueue(maxSize);
	printf("poczatek\n");

    // Set up threads (4 subscribers and a writer)
    pthread_t writer, subscriber1, subscriber2, subscriber3, subscriber4;
    pthread_create(&subscriber1, NULL, readerFunc2, queue);
    pthread_create(&subscriber2, NULL, readerFunc2, queue);
    pthread_create(&subscriber3, NULL, readerFunc2, queue);
    pthread_create(&subscriber4, NULL, readerFunc2, queue);
    pthread_create(&writer, NULL, writerFunc2, queue);

    pthread_join(subscriber1, NULL);
    pthread_join(subscriber2, NULL);
    pthread_join(subscriber3, NULL);
    pthread_join(subscriber4, NULL);
    pthread_join(writer, NULL);

    // Destroy the queue
    destroyQueue(queue);

    return 0;
}
