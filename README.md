
# Publish - Subscribe
## Programowanie systemowe i współbieżne
### Wojciech Anioł 160137 wojciech.aniol@student.put.poznan.pl
### Data:     v1.0, 25.01.2025
### Repozytorium: [Clikcable-link](https://github.com/wojciechaniol/psw-project)

## Struktury danych
### TQueue
Główną strukturą jest struktura kolejki cyklicznej zaprezentowana poniżej:
```C
typedef struct TQueue
{
    void** messages; // Tablica wskaźników na wiadomości
    int maxSize; // Rozmiar kolejki
    int currentSize; // Liczba aktualnie składowanych wiadomości
    int head; // Indeks najstarszego elementu
    int tail; // Indeks najnowszego elementu
    int* recipients; // Tablica liczby odbiorców poszczególnych wiadomości
    pthread_mutex_t lock; // Globalny zamek
    pthread_cond_t isFull; // Zmienna warunkowa sprawdzająca czy liczba wiadomości w kolejce jest równa jej rozmiarowi
    pthread_cond_t newMessages; // Zemienna warunkowa sprawdzająca czy powstała nowa wiadomość
    int subscribersCount; // Zmienna zawierająca informacje o liczbie subskrybentów
    int subscribersSize; // Limit subskrybentów
    Subscriber* subscribers; // Tablica zawierająca informacje o poszczególnych subskrybentach
} TQueue;
```
### Subscriber
Kolejną istotną strukturą danych jest `Subscriber`, który opisuje poszczególnego subskrybenta:
```C
typedef struct Subscriber
{
    pthread_t* subscriberThread; // Wskaźnik na przypisany subskrybentowi wątek
    int msgesToRead; // Indeks następnej nieodczytanej wiadomości
    int availableMessages; // Liczba dostępnych wiadomości
} Subscriber;
```
## Funkcje 
Poniżej opisane są funkcje, które zostaną użyte w implementacji:
- `TQueue* createQueue(int size)` - inicjuje strukturę TQueue reprezentującą nową kolejkę o początkowym, maksymalnym rozmiarze size.
- `void destroyQueue(TQueue *queue)` - usuwa kolejkę queue i zwalnia pamięć przez nią zajmowaną. Próba dostarczania lub odbioru nowych wiadomości z takiej kolejki będzie kończyła się błędem.
- `void subscribe(TQueue *queue, pthread_t thread)` - rejestruje wątek `thread` jako kolejnego odbiorcę wiadomości z kolejki queue.
- `void unsubscribe(TQueue *queue, pthread_t thread)` - wyrejestrowuje wątek thread z kolejki queue. Nieodebrane przez wątek wiadomości są traktowane jako odebrane.
- `void addMsg(TQueue *queue, void *msg)` - wstawia do kolejki `queue` nową wiadomość reprezentowaną wskaźnikiem msg.
- `void* getMsg(TQueue *queue, pthread_t thread)` - odbiera pojedynczą wiadomość z kolejki `queue` dla wątku `thread`. Jeżeli nie ma nowych wiadomości, funkcja jest blokująca. Jeżeli wątek `thread` nie jest zasubskrybowany – zwracany jest pusty wskaźnik NULL
- `int getAvailable(TQueue *queue, pthread_t thread)` - zwraca liczbę wiadomości z kolejki `queue` dostępnych dla wątku `thread`.
- `void removeMsg(TQueue *queue, void *msg)` - usuwa wiadomość `msg` z kolejki.
- `void setSize(TQueue *queue, int size)` - ustala nowy, maksymalny rozmiar kolejki. Jeżeli nowy rozmiar jest mniejszy od aktualnej liczby wiadomości w kolejce, to nadmiarowe wiadomości są usuwane z kolejki, począwszy od najstarszych.
- `int subscriberSearch(TQueue* queue, pthread_t thread)` - dopasowuje numer wątku do subskrybenta.
## Algorytm
### Założenia
Algorytm Publish-Subscribe ma działać w następujący sposób:
1. Z perspektywy kolejki:
   1. Kolejka może posiadać subskrybentów lub ich nie posiadać.
   2. Jeżeli kolejka nie posiada subskrybentów, każda wysyłana wiadomość zostaje natychmiastowo usunięta.
   3. Jeżeli zaś kolejka posiada subskrybentów, wiadomość usuwana jest dopiero po odczytaniu jej przez wyszstykich subskrybentów, dla których jest ona dostępna.
   4. Jeżeli buffer kolejki zostanie zapełniony, wątek publikujący nie jest w stanie wysłać więcej wiadomości -- musi odczekać aż któraś z nich zosatnie usunięta.

2. Z perspektywy subskrybenta:
   1. Dany subskrybent może otrzymać wiadomości, które zostały wstawione po jego subskrybcji.
   2. Jeżeli wątek nie subskrybuje kolejki otrzymuje NULL w odpowidziach na prośbę o wiadomość.
   3. Jeżeli wątek odebrał już wszystkie dostępne wiadomości, każda kolejna prośba o wiadomość jest działaniem blokującym.

### Użyte mechanizmy
Poniżej opisane są mechanizmy, które zostały użyte w opiswyanym algorytmie:
1. Globalny mutex `lock`, który znajduje się w implementacji prawie wszystkich funkcji i chroni współdzielone zasoby kolejki, takie jak: wiadomości, wartości `head` oraz `tail`, tablicę subskrybentów, pomocniczą tablicę odbiorców i zmienne dotyczące rozmiaru kolejki czy liczby subskrybentów. Użycie pojdeynczego zamka ułatwia implementację i zapobiega działaniom blokującym, ponieważ praca publikującego i czytelników może następować sekwencyjnie - raz do kolejki dostęp mają czytelnicy, raz publikujący.
2. Zmienna warunkowa `isFull` strzeże możliwości dodania więcej wiadomości niż jest miejsc na nie w kolejce. Brak miejsca w kolejce skutkuje uśpieniem wątku piszącego. Wybudzenie następuje po usunięciu którejś z wiadomości.
3. Zmienna warunkowa `newMessages` służy jako zabezpieczenie na wypadek braku dostępnych wiadomości w kolejce, które mogłyby zostać odczytane przez czytelników. Jeżeli nie ma wiadomości, wątki czytelników próbujące odebrać wiadomość zostają uśpione i dopiero dodanie nowej wiadomości skutkuje wysłaniem sygnału o dostępności.
