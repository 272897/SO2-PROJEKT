# README

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
