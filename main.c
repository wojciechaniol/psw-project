#include "queue.h"

void* writerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    int* i = 0, j = 0;

    while(1)
    {
		printf("writer j: %d\n", j);
        addMsg(queue, i++);
        //setSize(queue, (newSize+queue->maxSize));
        addMsg(queue, i++);
        j++;
        if (j >= 15)
        {
            break;
        }
    }

    return NULL;
}

void* readerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, &self);
	int j = 0;

    while(j < 10)
    {
		printf("recieving %d\n", j);
		getMsg(queue, &self);
		j++;
    }
	unsubscribe(queue, &self);

    return NULL;
}

int main() 
{
    // Initialize a queue with a maximum size
    int maxSize = 5;
    TQueue* queue;
    queue = createQueue(maxSize);

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
