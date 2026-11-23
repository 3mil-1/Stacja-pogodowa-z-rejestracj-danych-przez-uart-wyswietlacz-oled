// generator_ramek.cpp
// Generator ramek stacji pogodowej: narzędzie testowe dla dekodera na PC.
// Udaje płytkę: co "sekundę" wysyła trzy ramki (temperatura, wilgotność,
// ciśnienie) i opcjonalnie celowo psuje część z nich.
//
// Format ramki (liczby wielobajtowe: najmłodszy bajt pierwszy):
//   [0]      0xAA            bajt startu
//   [1]      typ             0x01 temperatura, 0x02 wilgotność, 0x03 ciśnienie
//   [2]      numer kolejny   0..255, potem od nowa
//   [3..6]   czas [ms]       uint32, czas od startu płytki
//   [7]      N               długość danych w bajtach
//   [8..]    dane            N bajtów:
//                              temperatura: int16,  setne °C   (2140 = 21,40 °C)
//                              wilgotność:  uint16, setne %    (4520 = 45,20 %)
//                              ciśnienie:   uint32, Pa         (101325 = 1013,25 hPa)
//   [ostatni] suma           (suma bajtów od typu do ostatniego bajtu danych) mod 256
//
// Kompilacja: g++ -std=c++17 -O2 -o generator generator_ramek.cpp
// Użycie:     generator [sekundy=10] [p_bledu=0.0] [plik=ramki.bin] [ziarno=1]
//   sekundy  - ile sekund pracy płytki zasymulować (3 ramki na sekundę)
//   p_bledu  - prawdopodobieństwo, że dana ramka zostanie zepsuta (0.0 - 1.0)
//   plik     - plik binarny, do którego trafią wszystkie bajty strumienia
//   ziarno   - ziarno losowania; to samo ziarno daje zawsze ten sam strumień

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace ramka {
constexpr uint8_t START = 0xAA;

enum class Typ : uint8_t {
    Temperatura = 0x01,
    Wilgotnosc  = 0x02,
    Cisnienie   = 0x03,
};

const char* nazwa(Typ typ) {
    switch (typ) {
        case Typ::Temperatura: return "temperatura";
        case Typ::Wilgotnosc:  return "wilgotnosc";
        case Typ::Cisnienie:   return "cisnienie";
    }
    return "?";
}
}  // namespace ramka

// Składa poprawne ramki. Nic nie losuje i niczego nie psuje.
class KoderRamek {
public:
    std::vector<uint8_t> zakoduj(ramka::Typ typ, uint8_t numer, uint32_t czasMs,
                                 const std::vector<uint8_t>& dane) const {
        std::vector<uint8_t> r;
        r.push_back(ramka::START);
        r.push_back(static_cast<uint8_t>(typ));
        r.push_back(numer);
        dopiszLE(r, czasMs, 4);
        r.push_back(static_cast<uint8_t>(dane.size()));
        r.insert(r.end(), dane.begin(), dane.end());

        // Suma od typu (indeks 1) do końca danych. Zmienna 8-bitowa
        // sama "przekręca się" po 255, więc to od razu jest modulo 256.
        uint8_t suma = 0;
        for (std::size_t i = 1; i < r.size(); ++i) {
            suma = static_cast<uint8_t>(suma + r[i]);
        }
        r.push_back(suma);
        return r;
    }

    std::vector<uint8_t> temperatura(uint8_t numer, uint32_t czasMs, double stopnie) const {
        const auto setne = static_cast<int16_t>(std::lround(stopnie * 100.0));
        std::vector<uint8_t> dane;
        // int16 -> uint16: te same bity (kod U2), tylko inaczej interpretowane.
        dopiszLE(dane, static_cast<uint16_t>(setne), 2);
        return zakoduj(ramka::Typ::Temperatura, numer, czasMs, dane);
    }

    std::vector<uint8_t> wilgotnosc(uint8_t numer, uint32_t czasMs, double procent) const {
        const auto setne = static_cast<uint16_t>(std::lround(procent * 100.0));
        std::vector<uint8_t> dane;
        dopiszLE(dane, setne, 2);
        return zakoduj(ramka::Typ::Wilgotnosc, numer, czasMs, dane);
    }

    std::vector<uint8_t> cisnienie(uint8_t numer, uint32_t czasMs, double paskale) const {
        const auto pa = static_cast<uint32_t>(std::lround(paskale));
        std::vector<uint8_t> dane;
        dopiszLE(dane, pa, 4);
        return zakoduj(ramka::Typ::Cisnienie, numer, czasMs, dane);
    }

private:
    // Dopisuje ileBajtow najmłodszych bajtów liczby x, od najmłodszego.
    static void dopiszLE(std::vector<uint8_t>& v, uint32_t x, int ileBajtow) {
        for (int i = 0; i < ileBajtow; ++i) {
            v.push_back(static_cast<uint8_t>(x >> (8 * i)));
        }
    }
};

// Udaje płytkę: generuje wiarygodne pomiary i z zadanym prawdopodobieństwem
// psuje ramki, żeby dekoder miał co robić.
class SymulatorStacji {
public:
    struct Statystyki {
        int wyslane = 0;          // ramki, które w ogóle wyszły (także zepsute)
        int poprawne = 0;         // ramki wysłane bez żadnej ingerencji
        int uszkodzonyBajt = 0;
        int uciete = 0;
        int smieciPrzed = 0;      // poprawna ramka, ale przed nią losowe bajty
        int zgubione = 0;         // ramka w ogóle nie wyszła (dziura w numeracji)
    };

    SymulatorStacji(double pBledu, uint32_t ziarno) : pBledu_(pBledu), los_(ziarno) {}

    // Bajty wysłane przez płytkę w ciągu jednej sekundy: trzy ramki, jedna po drugiej.
    std::vector<uint8_t> nastepnaSekunda(std::ostream& log) {
        const double t = czasMs_ / 1000.0;  // czas w sekundach
        constexpr double PI = 3.14159265358979323846;
        std::normal_distribution<double> szum(0.0, 1.0);

        // Wolne zmiany (okres 10 minut) plus mały szum, żeby wykres nie był płaski.
        const double temp = 21.0 + 2.0 * std::sin(2 * PI * t / 600.0) + 0.05 * szum(los_);
        const double wilg = 45.0 + 5.0 * std::sin(2 * PI * t / 900.0) + 0.20 * szum(los_);
        const double cisn = 101325.0 + 150.0 * std::sin(2 * PI * t / 1200.0) + 3.0 * szum(los_);

        std::vector<uint8_t> wyjscie;
        dodaj(wyjscie, koder_.temperatura(numer_++, czasMs_, temp), log);
        dodaj(wyjscie, koder_.wilgotnosc(numer_++, czasMs_, wilg), log);
        dodaj(wyjscie, koder_.cisnienie(numer_++, czasMs_, cisn), log);

        czasMs_ += 1000;
        return wyjscie;
    }

    const Statystyki& statystyki() const { return stat_; }

private:
    void dodaj(std::vector<uint8_t>& wyjscie, std::vector<uint8_t> r, std::ostream& log) {
        const int numer = r[2];
        const auto typ = static_cast<ramka::Typ>(r[1]);
        std::uniform_real_distribution<double> rownomierny(0.0, 1.0);

        if (rownomierny(los_) >= pBledu_) {
            wyjscie.insert(wyjscie.end(), r.begin(), r.end());
            ++stat_.wyslane;
            ++stat_.poprawne;
            return;
        }

        log << "  [blad] ramka nr " << numer << " (" << ramka::nazwa(typ) << "): ";
        switch (losuj(0, 3)) {
            case 0: {  // jeden bajt (poza startem) zmieniony na inny
                const int i = losuj(1, static_cast<int>(r.size()) - 1);
                const auto maska = static_cast<uint8_t>(losuj(1, 255));
                r[i] ^= maska;
                log << "zmieniony bajt na pozycji " << i << '\n';
                ++stat_.uszkodzonyBajt;
                break;
            }
            case 1: {  // płytka przestała nadawać w połowie ramki
                const int dlugosc = losuj(1, static_cast<int>(r.size()) - 1);
                r.resize(dlugosc);
                log << "ucieta po " << dlugosc << " bajtach"
                    << " (dekoder moze przez nia zgubic tez nastepna ramke)\n";
                ++stat_.uciete;
                break;
            }
            case 2: {  // losowe bajty przed poprawną ramką, czasem z fałszywym startem
                const int ile = losuj(1, 6);
                std::vector<uint8_t> smieci;
                for (int k = 0; k < ile; ++k) smieci.push_back(static_cast<uint8_t>(losuj(0, 255)));
                if (losuj(0, 1) == 1) smieci[losuj(0, ile - 1)] = ramka::START;
                r.insert(r.begin(), smieci.begin(), smieci.end());
                log << ile << " bajtow smieci przed ramka (sama ramka poprawna)\n";
                ++stat_.smieciPrzed;
                break;
            }
            case 3:  // ramka w ogóle nie wyszła
                log << "zgubiona\n";
                ++stat_.zgubione;
                return;
        }
        wyjscie.insert(wyjscie.end(), r.begin(), r.end());
        ++stat_.wyslane;
    }

    int losuj(int od, int doWlacznie) {
        return std::uniform_int_distribution<int>(od, doWlacznie)(los_);
    }

    KoderRamek koder_;
    double pBledu_;
    std::mt19937 los_;
    uint8_t numer_ = 0;     // przekręca się sam po 255
    uint32_t czasMs_ = 0;
    Statystyki stat_;
};

static void wypiszHex(const std::vector<uint8_t>& bajty) {
    for (uint8_t b : bajty) std::printf("%02X ", b);
    std::printf("\n");
}

// Sprawdza koder na przykładzie policzonym ręcznie w rozmowie.
static bool testKodera() {
    const std::vector<uint8_t> oczekiwana = {0xAA, 0x01, 0x07, 0xE8, 0x03, 0x00,
                                             0x00, 0x02, 0x5C, 0x08, 0x59};
    const auto wynik = KoderRamek{}.temperatura(7, 1000, 21.40);
    if (wynik != oczekiwana) {
        std::printf("TEST KODERA NIE PRZESZEDL. Wyszlo:   ");
        wypiszHex(wynik);
        std::printf("                           Oczekiwano: ");
        wypiszHex(oczekiwana);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    if (!testKodera()) return 1;

    const int sekundy     = argc > 1 ? std::atoi(argv[1]) : 10;
    const double pBledu   = argc > 2 ? std::atof(argv[2]) : 0.0;
    const std::string plik = argc > 3 ? argv[3] : "ramki.bin";
    const auto ziarno     = static_cast<uint32_t>(argc > 4 ? std::atoi(argv[4]) : 1);

    std::ofstream wyjscie(plik, std::ios::binary);
    if (!wyjscie) {
        std::cerr << "Nie moge otworzyc pliku " << plik << '\n';
        return 1;
    }

    SymulatorStacji symulator(pBledu, ziarno);
    std::size_t lacznieBajtow = 0;

    for (int s = 0; s < sekundy; ++s) {
        std::printf("t = %d s:\n", s);
        const auto bajty = symulator.nastepnaSekunda(std::cout);
        std::printf("  ");
        wypiszHex(bajty);
        wyjscie.write(reinterpret_cast<const char*>(bajty.data()),
                      static_cast<std::streamsize>(bajty.size()));
        lacznieBajtow += bajty.size();
    }

    const auto& st = symulator.statystyki();
    std::printf("\nZapisano %zu bajtow do %s\n", lacznieBajtow, plik.c_str());
    std::printf("Ramek poprawnych:            %d\n", st.poprawne);
    std::printf("Ze smieciami przed (dobre):  %d\n", st.smieciPrzed);
    std::printf("Ze zmienionym bajtem:        %d\n", st.uszkodzonyBajt);
    std::printf("Uciete:                      %d\n", st.uciete);
    std::printf("Zgubione (nie wyslane):      %d\n", st.zgubione);
    return 0;
}