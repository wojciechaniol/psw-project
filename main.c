#include "tqueue.h"

void* readerFunc(void* arg) 
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    int num = 7, randNum;
    randNum = rand() % num;
    subscribe(queue, &self);

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

        if (randNum < 2)
        {
            int *message = i;
            addMsg(queue, message);
        }
        else if (randNum < 5)
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
            setSize(queue, newSize+queue->maxSize);
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
    setSize(queue, newSize);

    sleep(5);
    destroyQueue(queue);

    return NULL;
}

void* writerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    int* i = 0, j = 0;
    int newSize = -2;
    for (j = 0; j < 90; j++);
    j = 0;

    while(1)
    {
        printf("WIELKIE JOT: %d\n", j);
        addMsg(queue, i++);
        //setSize(queue, (newSize+queue->maxSize));
        printf("Aktualny rozmiar i liczba wiadomości: %d %d\n", queue->maxSize, queue->currentSize);
        addMsg(queue, i++);
        addMsg(queue, i++);
        addMsg(queue, i++);
        addMsg(queue, i++);
        setSize(queue, newSize+queue->maxSize);
        addMsg(queue, i++);
        j++;
        if (j >= 15)
        {
            destroyQueue(queue);
            return NULL;
        }

        if (queue->maxSize == 1)
        {
            setSize(queue, 10);
        }
        
    }

    return NULL;
}

void* readerFunc2(void* arg)
{
    TQueue* queue = (TQueue*)arg;
    pthread_t self = pthread_self();
    int i;

    while(queue != NULL)
    {
        subscribe(queue, &self);
        for (i = 0; i < 2*self; i++)
        {
            printf("\t");
        }
        printf("Thread %lu has %d available messages\n", self, getAvailable(queue, &self));
        getMsg(queue, &self);
        unsubscribe(queue, &self);
        getMsg(queue, &self);
        unsubscribe(queue, &self);
        getMsg(queue, &self);
        subscribe(queue, &self);
        for (i = 0; i < 2*self; i++)
        {
            printf("\t");
        }
        printf("Thread %lu has %d available messages\n", self, getAvailable(queue, &self));
        getMsg(queue, &self);
        getMsg(queue, &self);
    }

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

/*
What needs to be checked:
setSize;
size of the queue -> most of the time we didnt even get to index 4 when the size was 5;
why removing happens when not everyone has read - e.g. third reader read one message twice - propably problem with subscribersearch and shifting subscribers;
*/