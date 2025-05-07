# README

# Wielowątkowy Serwer Czatu z Klientem

## Opis problemu

Projekt implementuje wielowątkowy serwer czatu wraz z klientem, umożliwiający komunikację tekstową między wieloma użytkownikami w czasie rzeczywistym. Serwer obsługuje wielu klientów jednocześnie, przydzielając każdemu połączeniu osobny wątek. Użytkownicy mogą wysyłać wiadomości, które są dystrybuowane do wszystkich podłączonych klientów. System utrzymuje również historię czatu, dzięki czemu nowi użytkownicy po dołączeniu mogą zobaczyć poprzednie wiadomości.

Główne wyzwania projektu obejmują:
- Równoczesne zarządzanie wieloma połączeniami klientów
- Synchronizacja dostępu do współdzielonych zasobów
- Bezpieczne dołączanie i odłączanie klientów
- Skuteczne rozpowszechnianie wiadomości pomiędzy użytkownikami
- Zarządzanie historią czatu
- Obsługa zamknięcia serwera i klienta

## Instrukcje uruchomienia

### Wymagania wstępne
- System operacyjny Windows
- Kompilator C++ obsługujący standard C++17
- MinGW z biblioteką ws2_32 (WinSock2)
- Make (opcjonalnie)

### Kompilacja
Możesz skompilować projekt za pomocą dołączonego pliku Makefile:

```
make all
```

Lub ręcznie za pomocą kompilatora:

```
g++ -Wall -std=c++17 -pthread -o chat_server.exe chat_server.cpp -lws2_32
g++ -Wall -std=c++17 -pthread -o chat_client.exe chat_client.cpp -lws2_32
```

### Uruchomienie serwera
```
chat_server.exe
```

Serwer domyślnie nasłuchuje na porcie 8888 i może obsłużyć do 50 klientów jednocześnie.

### Uruchomienie klienta
```
chat_client.exe <adres_ip_serwera>
```

Na przykład, aby połączyć się z serwerem uruchomionym na tym samym komputerze:
```
chat_client.exe 127.0.0.1
```

Po uruchomieniu klienta zostaniesz poproszony o podanie nazwy użytkownika, a następnie będziesz mógł wysyłać i odbierać wiadomości.

### Zamykanie
- Serwer można zamknąć naciskając Ctrl+C w oknie terminala
- Klient może się rozłączyć przez wpisanie `/quit` lub naciśnięcie Ctrl+C

## Wątki i ich reprezentacja

### Serwer
1. **Wątek główny**
   - Odpowiedzialny za inicjalizację serwera
   - Nasłuchuje nowych połączeń za pomocą funkcji `accept()`
   - Tworzy nowe wątki klientów dla każdego połączenia
   - Obsługuje sygnały zakończenia (CTRL+C, zamknięcie okna)

2. **Wątki klientów** (tworzone dla każdego połączenia)
   - Funkcja `handle_client(std::shared_ptr<Client> client)`
   - Każdy wątek obsługuje komunikację z jednym klientem
   - Odbiera wiadomości od przypisanego klienta
   - Przetwarza polecenia klienta (np. `/quit`)
   - Rozpowszechnia wiadomości do innych klientów
   - Czyści zasoby po rozłączeniu klienta

### Klient
1. **Wątek główny**
   - Inicjalizuje połączenie z serwerem
   - Obsługuje wprowadzanie danych przez użytkownika
   - Wysyła wiadomości do serwera
   - Obsługuje sygnały zakończenia (CTRL+C, zamknięcie okna)

2. **Wątek odbierający** (`receive_messages()`)
   - Nasłuchuje i odbiera wiadomości przychodzące od serwera
   - Wyświetla te wiadomości użytkownikowi
   - Wykrywa rozłączenie serwera

## Sekcje krytyczne i ich rozwiązania

### 1. Dostęp do listy klientów
**Problem**: Wiele wątków próbuje jednocześnie modyfikować listę klientów (dodawanie, usuwanie, przeszukiwanie).

**Rozwiązanie**: Użycie muteksu `clients_mutex` do synchronizacji dostępu:
```cpp
std::mutex clients_mutex;
```

**Zastosowanie**:
- Blokowanie dostępu podczas dodawania nowego klienta
- Blokowanie podczas usuwania klienta po rozłączeniu
- Blokowanie podczas przeszukiwania listy w celu wysłania wiadomości do wszystkich

### 2. Ograniczenie liczby klientów
**Problem**: Potrzeba ograniczenia maksymalnej liczby równoczesnych połączeń.

**Rozwiązanie**: Implementacja własnego semafora (`Semaphore`), który kontroluje dostępność slotów dla klientów:
```cpp
class Semaphore {
private:
    std::mutex mutex;
    std::condition_variable condition;
    unsigned int count;
public:
    Semaphore(unsigned int initial_count) : count(initial_count) {}
    void wait(); // Dekrementacja licznika, blokuje gdy count == 0
    void post(); // Inkrementacja licznika, powiadamia jeden czekający wątek
};
```

**Zastosowanie**:
- Wątek główny wywołuje `connection_semaphore.wait()` przed akceptacją nowego klienta
- Wątek klienta wywołuje `connection_semaphore.post()` po rozłączeniu

### 3. Dostęp do historii czatu
**Problem**: Wiele wątków może jednocześnie modyfikować i odczytywać historię czatu.

**Rozwiązanie**: Użycie tego samego muteksu `clients_mutex` do synchronizacji dostępu do historii czatu:
```cpp
std::vector<std::string> chat_history;
```

**Zastosowanie**:
- Blokowanie podczas dodawania nowej wiadomości do historii
- Blokowanie podczas wysyłania historii do nowo podłączonego klienta

### 4. Wypisywanie na konsolę
**Problem**: W kliencie, wątek główny i wątek odbierający mogą jednocześnie próbować wypisywać dane na konsolę.

**Rozwiązanie**: Użycie dedykowanego muteksu `cout_mutex` dla operacji na konsoli:
```cpp
std::mutex cout_mutex;
```

**Zastosowanie**:
- Blokowanie podczas wypisywania komunikatów o błędach
- Blokowanie podczas wypisywania odebranych wiadomości

### 5. Flaga stanu działania
**Problem**: Bezpieczna sygnalizacja zakończenia działania programu między wątkami.

**Rozwiązanie**: Użycie zwykłych zmiennych typu bool z muteksami dla bezpiecznego dostępu:
```cpp
bool server_running = true;       // W serwerze
std::mutex server_running_mutex;  // Mutex dla server_running

bool running = true;              // W kliencie
std::mutex running_mutex;         // Mutex dla running
```

**Zastosowanie**:
- Bezpieczna modyfikacja i odczyt flagi `server_running` przy użyciu muteksu `server_running_mutex`
- Bezpieczna modyfikacja i odczyt flagi `running` przy użyciu muteksu `running_mutex`
- Sygnalizacja zakończenia między wątkami z synchronizacją dostępu

### 6. Licznik klientów
**Problem**: Bezpieczne śledzenie liczby aktywnych klientów.

**Rozwiązanie**: Użycie zwykłej zmiennej `client_count` z dedykowanym muteksem:
```cpp
int client_count = 0;
std::mutex client_count_mutex;
```

**Zastosowanie**:
- Bezpieczna inkrementacja licznika przy podłączeniu nowego klienta
- Bezpieczna dekrementacja licznika przy rozłączeniu klienta

## Podsumowanie

Projekt demonstruje zastosowanie technik programowania wielowątkowego w C++ do implementacji sieciowego systemu czatu. Dzięki zastosowaniu odpowiednich mechanizmów synchronizacji (mutex, zmienne warunkowe, semafory), system efektywnie zarządza współdzielonymi zasobami i zapewnia bezpieczną komunikację między wieloma klientami jednocześnie.

# Problem jedzących filozofów
## Opis projektu

Ten projekt to implementacja problemu ucztujących filozofów, która zapewnia synchronizację i zapobiega zakleszczeniom oraz zagłodzeniu. Program symuluje filozofów siedzących przy stole, którzy na zmianę myślą i jedzą, korzystając ze wspólnych widelców. Synchronizacja dostępu do zasobów odbywa się przy użyciu semaforów, mutexów i mechanizmu monitorów, co pozwala na jednoczesne jedzenie maksymalnie N-1 filozofów.

## Jak uruchomić?

### Wymagania

Kompilator obsługujący C++20 (np. g++)

System operacyjny obsługujący wątki POSIX

### Kompilacja

```bash
g++ -std=c++20 -o main main.cpp -lpthread
```

### Uruchomienie
```bash
./main <liczba_filozofów>
```
Przykład dla 5 filozofów:
```bash
./main 5
```
Liczba filozofów musi być większa niż 1.

## Opis problemu

Problem ucztujących filozofów przedstawia grupę filozofów siedzących wokół stołu, gdzie każdy ma do dyspozycji jeden widelec po lewej i jeden po prawej stronie. Aby zjeść posiłek, filozof musi zdobyć oba widelce. Jeśli synchronizacja dostępu do widelców nie jest poprawnie rozwiązana, może dojść do zakleszczenia (deadlock) lub zagłodzenia (starvation).

## Wątki i ich rola

### Wątki

Każdy filozof działa w osobnym wątku.

Cykl życia filozofa: myślenie → czekanie na widelce → jedzenie → odkładanie widelców.

### Sekcje krytyczne i ich rozwiązanie

#### Dostęp do widelców:

Zarządza nim klasa Waiter, która dba o synchronizację i kontroluje dostęp filozofów do widelców.

Filozof może podnieść oba widelce tylko wtedy, gdy są one dostępne.

Do synchronizacji dostępu do widelców używane są mutex oraz condition_variable, co zapewnia wzajemne wykluczanie i komunikację między wątkami.

#### Unikanie zakleszczenia:

Ograniczenie liczby jednocześnie jedzących filozofów do N-1 przy pomocy semafora my_counting_semaphore.

Zapobiega sytuacji, w której wszyscy filozofowie podniosą jeden widelec i zablokują się nawzajem.

#### Zapobieganie zagłodzeniu:

Kolejność dostępu do widelców ustalana jest na podstawie czasu zgłoszenia chęci jedzenia.

Dzięki temu filozofowie, którzy czekają najdłużej, mają pierwszeństwo w zdobyciu widelców.

mutex używany do operacji na strukturze danych przechowującej oczekujących filozofów zapobiega niepożądanym stanom wyścigu.

## Podsumowanie

Implementacja korzysta z semaforów, mutexów i monitorów, aby zapewnić sprawiedliwy dostęp do zasobów i uniknąć problemów zakleszczenia oraz zagłodzenia. Program działa stabilnie nawet dla dużej liczby filozofów.
