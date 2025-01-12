#include "tqueue.h"

// Include the declarations for your provided functions here or assume they are included.

void* readerFunc(void* arg) 
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    int num = 7, randNum;
    randNum = rand() % num;
    
    while (queue != NULL) 
    {
        randNum = rand() % num;

        if (randNum == 0 || randNum == 4)
        {
            void* message = getMsg(queue, &self);
        }
        else if (randNum == 1)
        {
            printf("Thread %lu has %d available messages\n", self, getAvailable(queue, &self));
        }
        else if (randNum == 2)
        {
            sleep(3);
        }
        else if (randNum == 3 || randNum == 6)
        {
            subscribe(queue, &self);
        }
        else
        {
            unsubscribe(queue, &self);
        }
    }
    return NULL;
}

void* writerFunc(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    int num = 5, randNum, i;

    for (i = 0; i < 10; i++)
    {
        randNum = rand() % num;

        if (randNum > 0 && randNum < 4)
        {
            int *message = i;
            addMsg(queue, message);
        }
        else if (randNum == 0)
        {
            int j, newSize, randSize = rand()%num;
            for (j = 0; j < randSize; j++)
            {
                newSize = randSize*(-1);
            }
            if (newSize+queue->maxSize < 0)
            {
                newSize *= -1;
            }
            setSize(queue, &newSize);
        }
        else
        {
            sleep(3);
        }
    }

    int j, newSize, randSize = rand()%num+1;
    for (j = 0; j < randSize; j++)
    {
        newSize = randSize*(-1);
    }

    newSize = queue->maxSize+newSize;

    if (newSize < 0)
    {
        newSize *= -1;
    }

    printf("newSize: %d\n", newSize);
    setSize(queue, &newSize);

    sleep(5);
    destroyQueue(queue);

    return NULL;
}

int main() 
{
    // Initialize a queue with a maximum size
    int maxSize = 5;
    TQueue queue;
    createQueue(&queue, &maxSize);

    // Set up threads (4 subscribers and a writer)
    pthread_t writer, subscriber1, subscriber2, subscriber3, subscriber4;
    pthread_create(&writer, NULL, writerFunc, &queue);
    pthread_create(&subscriber1, NULL, readerFunc, &queue);
    pthread_create(&subscriber2, NULL, readerFunc, &queue);
    pthread_create(&subscriber3, NULL, readerFunc, &queue);
    pthread_create(&subscriber4, NULL, writerFunc, &queue);

    pthread_join(subscriber1, NULL);
    pthread_join(subscriber2, NULL);
    pthread_join(subscriber3, NULL);
    pthread_join(subscriber4, NULL);
    pthread_join(writer, NULL);

    // Destroy the queue
    destroyQueue(&queue);

    return 0;
}

/*
What needs to be checked:
setSize;
size of the queue -> most of the time we didnt even get to index 4 when the size was 5;
why removing happens when not everyone has read - e.g. third reader read one message twice - propably problem with subscribersearch and shifting subscribers;
*/