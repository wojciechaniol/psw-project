#include "queue.h"

void* writerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    int j = 0, size = 1;
    char* letter = "ABCDEFGHIJKLMNOPRSTUWYZabcdefghijklmnoprstuwyz";

    while(1)
    {
        addMsg(queue, letter++);
        addMsg(queue, letter++);
        removeMsg(queue, &letter);
        addMsg(queue, letter++);
        j++;
        if (j >= 15)
        {
            break;
        }
        setSize(queue, queue->maxSize+size);
    }

    return NULL;
}

void* readerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    subscribe(queue, self);
	int j = 0;

    while(1)
    {
		// getMsg(queue, self);
        // getMsg(queue, self);
		j++;
        if (j == 20) 
        {
            unsubscribe(queue, self);
            break;
        }
    }
	unsubscribe(queue, self);

    return NULL;
}

int main() 
{
    // Initialize a queue with a maximum size
    int maxSize = 2;
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
