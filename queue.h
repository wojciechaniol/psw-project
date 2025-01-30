#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

typedef struct Subscriber
{
    pthread_t subscriberThread;
    int msgesToRead;
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
    pthread_cond_t isFull; // Conditional variable to check wheter is there any slot to send a message
    pthread_cond_t newMessages; // Conditional variable for subscribers for them to wait if they have no message available
    int subscribersCount;
    int subscribersSize;
    Subscriber* subscribers;
} TQueue;

TQueue* createQueue(int size); //inicjuje strukturę TQueue reprezentującą nową kolejkę o początkowym, maksymalnym rozmiarze size.
void destroyQueue(TQueue *queue); //usuwa kolejkę queue i zwalnia pamięć przez nią zajmowaną. Próba dostarczania
//lub odbioru nowych wiadomości z takiej kolejki będzie kończyła się błędem.
void subscribe(TQueue *queue, pthread_t thread); //rejestruje wątek thread jako kolejnego odbiorcę wiadomości z kolejki queue.
void unsubscribe(TQueue *queue, pthread_t thread); //wyrejestrowuje wątek thread z kolejki queue. Nieodebrane przez wątek wiadomości są traktowane jako odebrane
void addMsg(TQueue *queue, void *msg); //wstawia do kolejki queue nową wiadomość reprezentowaną wskaźnikiem msg.
void* getMsg(TQueue *queue, pthread_t thread); //odbiera pojedynczą wiadomość z kolejki queue dla wątku thread. Jeżeli nie ma
//nowych wiadomości, funkcja jest blokująca. Jeżeli wątek thread nie jest zasubskrybowany – zwracany jest pusty wskaźnik NULL.
int getAvailable(TQueue *queue, pthread_t thread); //zwraca liczbę wiadomości z kolejki queue dostępnych dla wątku thread.
void removeMsg(TQueue *queue, void *msg); //usuwa wiadomość msg z kolejki.
void setSize(TQueue *queue, int size); 
// ustala nowy, maksymalny rozmiar kolejki. Jeżeli nowy rozmiar jest mniejszy od
// aktualnej liczby wiadomości w kolejce, to nadmiarowe wiadomości są usuwane
// z kolejki, począwszy od najstarszych.
int subscriberSearch(TQueue* queue, pthread_t thread); // matches thread number with subscriber's index

#endif
