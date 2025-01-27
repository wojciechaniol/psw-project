#include "queue.h"

TQueue* createQueue(int size)
{
    TQueue* newQueue = malloc(sizeof(TQueue));

    if (newQueue != NULL)
    {
        newQueue->maxSize = size;
        newQueue->currentSize = 0;
        newQueue->head = 0;
        newQueue->tail = 0;
        newQueue->recipients = malloc(sizeof(int) * (size));
        newQueue->messages = malloc(sizeof(void *) * (size));
        newQueue->subscribersCount = 0;
        newQueue->subscribersSize = 10;
        newQueue->subscribers = NULL;
        pthread_mutex_init(&newQueue->lock, NULL);
        pthread_cond_init(&newQueue->newMessages, NULL);
        sem_init(&newQueue->emptySlots, 0, newQueue->maxSize);
    }

    return newQueue;
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
    pthread_mutex_destroy(&queue->lock);

    free(queue);
}

int subscriberSearch(TQueue* queue, pthread_t* thread)
{
    int i, index = -1;

    for (i = 0; i < queue->subscribersCount; i++)
    {
        if(pthread_equal(*queue->subscribers[i].subscriberThread, *thread))
        {
            index = i;
            break;
        }
    }

    return index;
}

void subscribe(TQueue *queue, pthread_t *thread)
{
    pthread_mutex_lock(&queue->lock);
    int i;

    for (i = 0; i < queue->subscribersCount; i++)
    {
        if (pthread_equal(*queue->subscribers[i].subscriberThread, *thread))
        {
            // for (i = 0; i < 2*(*thread); i++)
            // {
            //     printf("\t");
            // }

            // printf("subscriber %lu already added\n", *thread);
			pthread_mutex_unlock(&queue->lock);
            return;
        }
    }

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
    for (i = 0; i < queue->maxSize; i++)
    {
        subscriber->msgesToRead[i] = -1;
    }

    subscriber->availableMessages = 0;

    queue->subscribersCount++;

    // for (i = 0; i < 2*(*thread); i++)
    // {
    //     printf("\t");
    // }
    
    // printf("Thread %lu has just subscribed\n", *thread);
    pthread_mutex_unlock(&queue->lock);
    return;
}

void unsubscribe(TQueue *queue, pthread_t *thread)
{
    int index, i;

    pthread_mutex_lock(&queue->lock);

    index = subscriberSearch(queue, thread);

    if (index == -1)
    {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    Subscriber* subscriber = &queue->subscribers[index];

    for (i = 0; i < subscriber->availableMessages; i++)
    {
        queue->recipients[subscriber->msgesToRead[i]]--;
        if (queue->recipients[subscriber->msgesToRead[i]] == 0)
        {
            removeMsg(queue, queue->messages[subscriber->msgesToRead[i]]);
        }
        subscriber->msgesToRead[i] = -1;
    }

    subscriber->availableMessages = 0;
    free(subscriber->msgesToRead);

    for (i = index; i < queue->subscribersCount - 1; i++)
    {
        queue->subscribers[i] = queue->subscribers[i+1];
    }

    queue->subscribersCount--;
    // for (i = 0; i < 2*(*thread); i++)
    // {
    //     printf("\t");
    // }
    // printf("Thread %lu is unsubscribing\n", *thread);

    pthread_mutex_unlock(&queue->lock);
    return;
}

void addMsg(TQueue *queue, void *msg)
{
    sem_wait(&queue->emptySlots); // Wait for an empty slot
    pthread_mutex_lock(&queue->lock); // entering critical section
    
    // printf("writer is trying to add a message\n");

    queue->currentSize++;
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
    // printf("Writer thread added new message: %d\n", (int*)msg);

    if (queue->subscribersCount == 0)
    {
        removeMsg(queue, msg);
    }
    else
    {
        pthread_cond_broadcast(&queue->newMessages);
    }

    pthread_mutex_unlock(&queue->lock); // leaving critical section
}

void* getMsg(TQueue *queue, pthread_t *thread)
{
    int lastRead, i = 0;
    pthread_mutex_lock(&queue->lock); // critical section
    int index = subscriberSearch(queue, thread);

    if (index == -1)
    {
        // for (i = 0; i < 2*(*thread); i++)
        // {
        //     printf("\t");
        // }
        // printf("NULL for %lu this time\n", *thread);
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    // for (i = 0; i < 2*(*thread); i++)
    // {
    //     printf("\t");
    // }
    // printf("Thread %lu is waitning for message\n", *thread);

    while (queue->subscribers[index].availableMessages == 0)
    {
        // for (i = 0; i < 2*(*thread); i++)
        // {
        //     printf("\t");
        // }

        // printf("waiting\n");
        pthread_cond_wait(&queue->newMessages, &queue->lock); // Wait till you get anything to read
    }

    // if there is any available message, take its index
    lastRead = queue->subscribers[index].msgesToRead[0];

    void* msg = queue->messages[lastRead];

    queue->recipients[lastRead]--;
    queue->subscribers[index].availableMessages--;

    for (i = 0; i < queue->subscribers[index].availableMessages; i++)
    {
        queue->subscribers[index].msgesToRead[i] = queue->subscribers[index].msgesToRead[i + 1];
    }

    // for (i = 0; i < 2*(*thread); i++)
    // {
    //     printf("\t");
    // }

    // printf("Thread %lu received message: %d\n", *thread, (int*)msg);

    if (queue->recipients[lastRead] == 0)
    {
        removeMsg(queue, msg);
    }

    pthread_mutex_unlock(&queue->lock); // leaving critical section


    return msg;
}

int getAvailable(TQueue *queue, pthread_t *thread)
{
    int index = subscriberSearch(queue, thread);
    if (index == -1)
    {
        return 0;
    }

    return queue->subscribers[index].availableMessages;
}

void removeMsg(TQueue *queue, void *msg)
{
    int i, found = 0, index;

    for (i = 0; i < queue->maxSize; i++)
    {
        if (queue->messages[i] == msg)
        {
            found = 1;
            index = i;
            break;
        }
    }

    if (found)
    {
        // printf("i am removing the message of index: %d and content: %d\n", index, queue->messages[index]);
        queue->messages[i] = NULL;
        queue->currentSize--;
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

    return;
}

void setSize(TQueue *queue, int size) 
{
    pthread_mutex_lock(&queue->lock); // enters critical section

    // printf("setSize\n");
    if (queue == NULL || size <= 0) 
    {
        perror("invalid input");
        pthread_mutex_unlock(&queue->lock);
        return; // Invalid input
    }

    int newSize = size, index, i, j;

    // printf("newsize: %d\n", newSize);

    if (newSize < queue->currentSize) 
    {
        // If the new size is smaller, remove excess messages starting from the oldest
        int messagesToRemove = queue->currentSize - newSize;
        for (i = 0; i < messagesToRemove; i++) 
        {
            // using free instead of removeMsg to avoid using the lock again - possible solution is more than one lock (maybe distinctive lock for each slot)
            queue->messages[queue->head] = NULL;

            for (j = 0; j < queue->subscribersCount; j++) 
            {
                Subscriber *subscriber = &queue->subscribers[j];
                
                if (subscriber->msgesToRead[0] == queue->head)
                {
                    int* newMsgesToRead = malloc(queue->maxSize * sizeof(int));
                    if (newMsgesToRead == NULL)
                    {
                        pthread_mutex_unlock(&queue->lock);
                        perror("newMsgs");
                        return;
                    }
                    memcpy(newMsgesToRead, &subscriber->msgesToRead[1], queue->maxSize * sizeof(int));
                    free(subscriber->msgesToRead);
                    subscriber->msgesToRead = newMsgesToRead;
                    subscriber->availableMessages--;
                }
            }

            // Update the head pointer and decrement current size
            queue->head = (queue->head + 1) % queue->maxSize;
            queue->currentSize--;
        }
    }

    void **newMessages = malloc(newSize * sizeof(void *));
    int *newRecipients = malloc(newSize * sizeof(int));

    if (newMessages == NULL || newRecipients == NULL) 
    {
        pthread_mutex_unlock(&queue->lock);
        perror("newMessages");
        return; 
    }

    for (i = 1; i <= queue->currentSize; ++i) 
    {
        index = (queue->head + i) % queue->maxSize;
        newMessages[i-1] = queue->messages[index];
        newRecipients[i-1] = queue->recipients[index];
    }

    // Free the old arrays
    free(queue->messages);
    free(queue->recipients);

    // printf("Writer thread is changing the max size form %d to %d\n", queue->maxSize, newSize);

    // Update queue properties
    queue->messages = newMessages;
    queue->recipients = newRecipients;
    queue->maxSize = newSize;
    queue->head = 0;
    queue->tail = queue->currentSize;

    sem_destroy(&queue->emptySlots);
    sem_init(&queue->emptySlots, 0, newSize - queue->currentSize);

    pthread_mutex_unlock(&queue->lock); // end of the critical section
}
