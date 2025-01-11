#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

typedef struct Subscriber
{
    pthread_t* subscriberThread;
    int* msgesToRead;
    int availableMessages;
} Subscriber;

typedef struct TQueue
{
    void** messages;
    int maxSize; // Size of the queue
    int currentSize; // Number of currently stored elements
    int head; // Oldest element
    int tail; // Newest element
    int* recipients;
    pthread_mutex_t lock; // Global lock to secure resources
    sem_t emptySlots; // Counting semaphores to keep track of produced and consumed elements
    sem_t filledSlots;
    int subscribersCount;
    int subscribersSize;
    Subscriber* subscribers;
} TQueue;

void createQueue(TQueue *queue, int *size); //inicjuje strukturę TQueue reprezentującą nową kolejkę o początkowym, maksymalnym rozmiarze size.
void destroyQueue(TQueue *queue); //usuwa kolejkę queue i zwalnia pamięć przez nią zajmowaną. Próba dostarczania
//lub odbioru nowych wiadomości z takiej kolejki będzie kończyła się błędem.
void subscribe(TQueue *queue, pthread_t *thread); //rejestruje wątek thread jako kolejnego odbiorcę wiadomości z kolejki queue.
void unsubscribe(TQueue *queue, pthread_t *thread); //wyrejestrowuje wątek thread z kolejki queue. Nieodebrane przez wątek wiadomości są traktowane jako odebrane
void addMsg(TQueue *queue, void *msg); //wstawia do kolejki queue nową wiadomość reprezentowaną wskaźnikiem msg.
void* getMsg(TQueue *queue, pthread_t *thread); //odbiera pojedynczą wiadomość z kolejki queue dla wątku thread. Jeżeli nie ma
//nowych wiadomości, funkcja jest blokująca. Jeżeli wątek thread nie jest zasubskrybowany – zwracany jest pusty wskaźnik NULL.
int getAvailable(TQueue *queue, pthread_t *thread); //zwraca liczbę wiadomości z kolejki queue dostępnych dla wątku thread.
void removeMsg(TQueue *queue, void *msg); //usuwa wiadomość msg z kolejki.
void setSize(TQueue *queue, int *size); 
// ustala nowy, maksymalny rozmiar kolejki. Jeżeli nowy rozmiar jest mniejszy od
// aktualnej liczby wiadomości w kolejce, to nadmiarowe wiadomości są usuwane
// z kolejki, począwszy od najstarszych.
int subscriberSearch(TQueue* queue, pthread_t* thread); // matches thread number with subscriber's index

void createQueue(TQueue *queue, int *size)
{
    queue->maxSize = *size;
    queue->currentSize = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->recipients = malloc(sizeof(int) * (*size));
    queue->messages = malloc(sizeof(void *) * (*size));
    queue->subscribersCount = 0;
    queue->subscribersSize = 10;
    queue->subscribers = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    sem_init(&queue->emptySlots, 0, queue->maxSize);
    sem_init(&queue->filledSlots, 0, 0);
}

void destroyQueue(TQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    if (queue->messages != NULL)
    {
        free(queue->messages);
    }

    if (queue->subscribers != NULL)
    {
        free(queue->subscribers);
    }

    sem_destroy(&queue->emptySlots);
    sem_destroy(&queue->filledSlots);
    pthread_mutex_destroy(&queue->lock);

    free(queue);
}

int subscriberSearch(TQueue* queue, pthread_t* thread)
{ // IT doesnt need mutex - mutex is provided by other functions
    int i, index = -1;
    pthread_mutex_lock(&queue->lock); // critical section

    for (i = 0; i < queue->subscribersCount; i++)
    {
        if(pthread_equal(*queue->subscribers[i].subscriberThread, *thread))
        {
            index = i;
            break;
        }
    }

    pthread_mutex_unlock(&queue->lock); // End of the critical section

    return index;
}

void subscribe(TQueue *queue, pthread_t *thread)
{
    pthread_mutex_lock(&queue->lock);
    if (queue->subscribers == NULL)
    {
        queue->subscribers = malloc(queue->subscribersSize * sizeof(Subscriber));
    }
    else if (queue->subscribersCount == queue->subscribersSize)
    {
        int newSize = queue->subscribersSize * 2;
        Subscriber* newSubscribers = realloc(queue->subscribers, newSize * sizeof(Subscriber));
        if (newSubscribers == NULL)
        {
            perror("newSubscribers"); //destroyQueue??
            pthread_mutex_unlock(&queue->lock);//czy to konieczne?
            return;//exit(0);
        }
        free(queue->subscribers);
        queue->subscribers = newSubscribers;
        queue->subscribersSize = newSize;
    }

    Subscriber* subscriber = &queue->subscribers[queue->subscribersCount];
    subscriber->subscriberThread = thread;
    subscriber->msgesToRead = malloc(queue->maxSize * sizeof(int));
    subscriber->availableMessages = 0;

    queue->subscribersCount++;

    pthread_mutex_unlock(&queue->lock);
}

void unsubscribe(TQueue *queue, pthread_t *thread)
{
    int index, i;
    index = subscriberSearch(queue, thread);

    pthread_mutex_lock(&queue->lock);

    Subscriber* subscriber = &queue->subscribers[index];

    while (subscriber->msgesToRead[i] != NULL)
    {
        queue->recipients[subscriber->msgesToRead[i]]--;
// i++
    }

    for (i = index; i < queue->subscribersCount - 1; i++)
    {
        queue->subscribers[i] = queue->subscribers[i+1];
    }

    queue->subscribersCount--;

    pthread_mutex_unlock(&queue->lock);
}

void addMsg(TQueue *queue, void *msg)
{
    sem_wait(&queue->emptySlots); // Wait for an empty slot
    pthread_mutex_lock(&queue->lock); // entering critical section
    queue->messages[queue->tail] = msg;
    queue->recipients[queue->tail] = queue->subscribersCount;

    // with every put message the number of available messages for currently subscribed users is incremented
    int i;
    for (i = 0; i < queue->subscribersCount; i++)
    {
        queue->subscribers[i].availableMessages++;
        queue->subscribers[i].msgesToRead[queue->subscribers[i].availableMessages-1] = queue->tail;
    }

    queue->tail = (queue->tail+1) % queue->maxSize;
    pthread_mutex_unlock(&queue->lock); // leaving critical section

    sem_post(&queue->filledSlots); // V to signal readers that a message is available
}

void* getMsg(TQueue *queue, pthread_t *thread)
{
    int index = subscriberSearch(queue, thread);
    int lastRead, i = 0;
    sem_wait(&queue->filledSlots); // Wait till you get anything to read
    pthread_mutex_lock(&queue->lock); // Second critical section

    if (queue->subscribers[index].msgesToRead[0] != NULL)
    {
        // if there is any available message, take its index
        lastRead = queue->subscribers[index].msgesToRead[0];
    }
    else
    {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    void* msg = NULL;

    msg = queue->messages[lastRead];
    queue->recipients[lastRead]--;
    queue->subscribers[index].availableMessages--;

    while (queue->subscribers[index].msgesToRead[i + 1] != NULL || i < queue->maxSize-1)
    {
        queue->subscribers[index].msgesToRead[i] = queue->subscribers[index].msgesToRead[i + 1];
    }

    queue->subscribers[index].msgesToRead[i] = NULL;

    pthread_mutex_unlock(&queue->lock); // End of second critical section

    return msg;
}

int getAvailable(TQueue *queue, pthread_t *thread)
{
    int index = subscriberSearch(queue, thread);

    return queue->subscribers[index].availableMessages;
}

void removeMsg(TQueue *queue, void *msg)
{
    int i, found = 0, index;
    pthread_mutex_lock(&queue->lock);

    for (i = 0; i < queue->maxSize; i++)
    {
        if (queue->messages[i] == msg)
        {
            queue->messages[i] = NULL;
            found = 1;
            index = i;
            break;
        }
    }

    if (found == 1)
    {
        queue->currentSize--;
        queue->recipients[index] = NULL;
        sem_post(&queue->emptySlots); // V to signal created empty slot
        
        if (index == queue->head)
        {
            if (queue->messages[(queue->head+1)%queue->maxSize] != NULL)
            {
                queue->head = (queue->head+1)%queue->maxSize;
            }
        }
        else if (index == queue->tail)
        {
            int previous = (queue->tail == 0) ? queue->maxSize-1 : queue->tail-1;
            if (queue->messages[previous] != NULL)
            {
                queue->tail = previous;
            }
        }
    }

    pthread_mutex_unlock(&queue->lock);
}

void setSize(TQueue *queue, int *size) 
{
    if (queue == NULL || size == NULL || *size <= 0) 
    {
        return; // Invalid input
    }

    pthread_mutex_lock(&queue->lock); // enters critical section

    int newSize = *size, index, i, j;

    if (newSize < queue->currentSize) 
    {
        // If the new size is smaller, remove excess messages starting from the oldest
        int messagesToRemove = queue->currentSize - newSize;
        for (i = 0; i < messagesToRemove; i++) 
        {
            // using free instead of removeMsg to avoid using the lock again - possible solution is more than one lock (maybe distinctive lock for each slot)
            free(queue->messages[queue->head]);

            for (j = 0; j < queue->subscribersCount; j++) 
            {
                Subscriber *subscriber = &queue->subscribers[j];
                
                if (subscriber->msgesToRead[0] == queue->head)
                {
                    int* newMsgesToRead = malloc(queue->maxSize * sizeof(int));
                    memcpy(newMsgesToRead, &subscriber->msgesToRead[1], queue->maxSize * sizeof(int));
                    free(subscriber->msgesToRead);
                    subscriber->msgesToRead = newMsgesToRead;
//subscriber->available--
                }
            }

            // Update the head pointer and decrement current size
            queue->head = (queue->head + 1) % queue->maxSize;
            queue->currentSize--;
        }
    }

    void **newMessages = malloc(newSize * sizeof(void *));
    int *newRecipients = malloc(newSize * sizeof(int));

    if (newMessages == NULL) 
    {
        pthread_mutex_unlock(&queue->lock);
        return; 
    }

    for (i = 0; i < queue->currentSize; ++i) 
    {
        index = (queue->head + i) % queue->maxSize;
        newMessages[i] = queue->messages[index];
        newRecipients[i] = queue->recipients[index];
    }

    // Free the old messages array
    free(queue->messages);
    free(queue->recipients);

    // Update queue properties
    queue->messages = newMessages;
    queue->recipients = newRecipients;
    queue->maxSize = newSize;
    queue->head = 0;
    queue->tail = queue->currentSize;

    sem_destroy(&queue->emptySlots);
    sem_destroy(&queue->filledSlots);
    sem_init(&queue->emptySlots, 0, newSize - queue->currentSize);
    sem_init(&queue->filledSlots, 0, queue->currentSize);

    pthread_mutex_unlock(&queue->lock); // end of the critical section
}