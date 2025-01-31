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
        pthread_cond_init(&newQueue->isFull, NULL);
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

    pthread_mutex_destroy(&queue->lock);

    free(queue);

    return;
}

int subscriberSearch(TQueue* queue, pthread_t thread)
{
    int i, index = -1;

    for (i = 0; i < queue->subscribersCount; i++)
    {
        if(pthread_equal(queue->subscribers[i].subscriberThread, thread))
        {
            index = i;
            break;
        }
    }

    return index;
}

void subscribe(TQueue *queue, pthread_t thread)
{
    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return;
    }

    pthread_mutex_lock(&queue->lock);

    if (queue->subscribers == NULL)
    {
        queue->subscribers = malloc(queue->subscribersSize * sizeof(Subscriber));
        if (queue->subscribers == NULL)
        {
            perror("malloc subscribers error");
            pthread_mutex_unlock(&queue->lock);
            return;
        }
    }
    else if (queue->subscribersCount == queue->subscribersSize)
    {
        int newSize = queue->subscribersSize * 2;
        Subscriber* newSubscribers = realloc(queue->subscribers, newSize * sizeof(Subscriber));
        if (newSubscribers == NULL)
        {
            perror("newSubscribers");
            pthread_mutex_unlock(&queue->lock);
            return;
        }
        queue->subscribers = newSubscribers;
        queue->subscribersSize = newSize;
    }

    if (subscriberSearch(queue, thread) > 0)
    {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    Subscriber* subscriber = &queue->subscribers[queue->subscribersCount];
    subscriber->subscriberThread = thread;
    subscriber->msgesToRead = queue->tail;

    subscriber->availableMessages = 0;

    queue->subscribersCount++;

    pthread_mutex_unlock(&queue->lock);
    return;
}

void unsubscribe(TQueue *queue, pthread_t thread)
{
    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return;
    }

    pthread_mutex_lock(&queue->lock);

    int index, i;

    index = subscriberSearch(queue, thread);

    if (index == -1)
    {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

    Subscriber* subscriber = &queue->subscribers[index];

    for (i = 0; i < subscriber->availableMessages; i++)
    {
        queue->recipients[subscriber->msgesToRead]--;
        if (queue->recipients[subscriber->msgesToRead] == 0)
        {
            internalRemove(queue, queue->messages[subscriber->msgesToRead]);
        }
        subscriber->msgesToRead = (subscriber->msgesToRead+1)%queue->maxSize;
    }

    subscriber->availableMessages = 0;

    for (i = index; i < queue->subscribersCount - 1; i++)
    {
        queue->subscribers[i] = queue->subscribers[i+1];
    }

    queue->subscribersCount--;

    pthread_mutex_unlock(&queue->lock);
    return;
}

void addMsg(TQueue *queue, void *msg)
{
    if (queue == NULL || msg == NULL)
    {
        perror("NULL pointer");
        return;
    }

    pthread_mutex_lock(&queue->lock); // entering critical section

    while (queue->maxSize == queue->currentSize)
    {
        pthread_cond_wait(&queue->isFull, &queue->lock);
    }

    queue->currentSize++;
    queue->messages[queue->tail] = msg;
    queue->recipients[queue->tail] = queue->subscribersCount;

    // with every put message the number of available messages for currently subscribed users is incremented
    int i;
    for (i = 0; i < queue->subscribersCount; i++)
    {
        queue->subscribers[i].availableMessages++;
    }

    queue->tail = (queue->tail+1) % queue->maxSize;

    if (queue->subscribersCount == 0)
    {
        internalRemove(queue, msg);
    }
    else
    {
        pthread_cond_broadcast(&queue->newMessages);
    }

    pthread_mutex_unlock(&queue->lock); // leaving critical section
    return;
}

void* getMsg(TQueue *queue, pthread_t thread)
{
    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return NULL;
    }

    pthread_mutex_lock(&queue->lock); // critical section
    int lastRead, index;

    while ((index = subscriberSearch(queue, thread)) != -1 
        && queue->subscribers[index].availableMessages == 0)
    {
        pthread_cond_wait(&queue->newMessages, &queue->lock); // Wait till you get anything to read
    }

    index = subscriberSearch(queue, thread);

    if (index == -1)
    {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    // if there is any available message, take its index
    lastRead = queue->subscribers[index].msgesToRead;

    void* msg = queue->messages[lastRead];

    queue->recipients[lastRead]--;
    queue->subscribers[index].availableMessages--;

    queue->subscribers[index].msgesToRead = (lastRead+1)%queue->maxSize;

    if (queue->recipients[lastRead] <= 0)
    {
        internalRemove(queue, msg);
    }

    pthread_mutex_unlock(&queue->lock); // leaving critical section

    return msg;
}

int getAvailable(TQueue *queue, pthread_t thread)
{
    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return -1;
    }

    pthread_mutex_lock(&queue->lock);

    int index = subscriberSearch(queue, thread);
    if (index == -1)
    {
        return 0;
    }

    pthread_mutex_unlock(&queue->lock);

    return queue->subscribers[index].availableMessages;
}

void internalRemove(TQueue *queue, void *msg)
{
    int i, found = 0, index;

    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return;
    }

    if (msg == NULL)
    {
        return;
    }

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
        queue->messages[index] = NULL;
        queue->currentSize--;
        
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

        pthread_cond_broadcast(&queue->isFull);
    }

    return;
}

void removeMsg(TQueue *queue, void *msg)
{
    int i, found = 0, index;

    pthread_mutex_lock(&queue->lock);

    if (queue == NULL)
    {
        perror("Queue NULL pointer");
        return;
    }

    if (msg == NULL)
    {
        pthread_mutex_unlock(&queue->lock);
        return;
    }

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
        queue->messages[index] = NULL;
        queue->currentSize--;
        
        for (i = 0; i < queue->subscribersCount; i++)
        {
            Subscriber* subscriber = &queue->subscribers[i];

            if ((subscriber->msgesToRead < index && index < queue->head)
            || (index < queue->head && queue->head <= subscriber->msgesToRead) 
            || (queue->head <= subscriber->msgesToRead && subscriber->msgesToRead < index)
            || subscriber->msgesToRead == index)
            {
                if (subscriber->availableMessages > 0)
                {
                    subscriber->availableMessages--;
                }

                if (subscriber->msgesToRead == queue->head && index == queue->head 
                && subscriber->availableMessages > 0)
                {
                    subscriber->msgesToRead = (subscriber->msgesToRead+1)%queue->maxSize;
                }
            }
            else if (index != queue->head && index != queue->tail)
            {
                if (subscriber->msgesToRead == 0)
                {
                    subscriber->msgesToRead = queue->maxSize - 1;
                }
                else
                {
                    subscriber->msgesToRead--;
                }
            }
        }

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
        else
        {
            i = index;

            while ((i+1)%queue->maxSize != queue->head)
            {
                queue->messages[i] = queue->messages[i+1];
                queue->recipients[i] = queue->recipients[i+1];
                i = (i+1)%queue->maxSize;

                if ((i+1)%queue->maxSize == queue->head) 
                {
                    queue->messages[i] = NULL;
                    queue->recipients[i] = 0;
                }
            }

            queue->tail = (queue->tail == 0) ? queue->maxSize-1 : queue->tail-1;
        }

        pthread_cond_broadcast(&queue->isFull);
    }

    pthread_mutex_unlock(&queue->lock);

    return;
}

void setSize(TQueue *queue, int size) 
{
    if (queue == NULL || size <= 0) 
    {
        perror("invalid input");
        return; // Invalid input
    }

    pthread_mutex_lock(&queue->lock); // enters critical section

    int newSize = size, index, i, j;

    if (newSize < queue->maxSize) 
    {
        // If the new size is smaller, remove excess messages starting from the oldest
        int messagesToRemove = queue->currentSize - newSize;

        for (i = 0; i < messagesToRemove; i++) 
        {
            // Using free instead of removeMsg to avoid using unnecessary operations on the message
            queue->messages[queue->head] = NULL;
            queue->recipients[queue->head] = 0;

            for (j = 0; j < queue->subscribersCount; j++) 
            {
                Subscriber *subscriber = &queue->subscribers[j];
                
                if (subscriber->msgesToRead == queue->head)
                {
                    if (subscriber->availableMessages > 0)
                    {
                        subscriber->availableMessages--;
                        subscriber->msgesToRead = (subscriber->msgesToRead+1) % queue->maxSize;
                    }
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

    for (i = 0; i < queue->currentSize; i++) 
    {
        index = (queue->head + i) % queue->maxSize;
        newMessages[i] = queue->messages[index];
        newRecipients[i] = queue->recipients[index];
    }

    for (i = 0; i < queue->subscribersCount; i++)
    {
        Subscriber *subscriber = &queue->subscribers[i];

        int messageIndex = queue->currentSize-subscriber->availableMessages;

        if (messageIndex < 0)
        {
            perror("Negative index\n");
        }

        subscriber->msgesToRead = messageIndex % newSize;
    }

    // Free the old arrays
    free(queue->messages);
    free(queue->recipients);

    queue->messages = newMessages;
    queue->recipients = newRecipients;
    queue->maxSize = newSize;
    queue->head = 0;
    queue->tail = queue->currentSize % newSize;

    if (queue->currentSize < queue->maxSize)
    {
        pthread_cond_broadcast(&queue->isFull);
    }

    pthread_mutex_unlock(&queue->lock); // end of the critical section
}
