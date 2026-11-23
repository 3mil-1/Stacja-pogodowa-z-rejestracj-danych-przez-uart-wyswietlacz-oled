#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <memory>
class Ramka{
    protected:
    std::vector<uint8_t> ramka_;
    Ramka (const std::vector<uint8_t>& ramka):ramka_(ramka){}
    uint16_t sklej_dwie(std::size_t indeks)const{
        return(static_cast<uint16_t> (ramka_[indeks]) |static_cast<uint16_t>(ramka_[indeks+1])<<8);
    }
    uint32_t sklej_cztery(std::size_t indeks)const{
        return(static_cast<uint32_t>(ramka_[indeks])| static_cast<uint32_t>(ramka_[indeks+1])<<8|static_cast<uint32_t>(ramka_[indeks+2])<<16|static_cast<uint32_t>(ramka_[indeks+3])<<24);
    }
    
    public:
    uint8_t typ()const{return ramka_[1];}
    uint8_t numer()const{return ramka_[2];}
    uint32_t czas()const{return sklej_cztery(3);}
    virtual double wartosc() const = 0;
    virtual std::string jednostka() const = 0;
    virtual ~Ramka() = default;
    void wypisz(std::ostream& wyjscie){
        wyjscie<<wartosc()<<" "<<jednostka()<<"\n";
    }

};
class Ramka_temp : public Ramka  {
    public:
    Ramka_temp (const std::vector<uint8_t>& ramka):Ramka(ramka){}
    double wartosc()const{return(static_cast<double>(static_cast<int16_t>(sklej_dwie(8))) / 100.0);}
    std::string jednostka()const{return("stopnie C");}
};
class Ramka_wilg : public Ramka{
    public:
    Ramka_wilg(const std::vector<uint8_t>& ramka):Ramka(ramka){}
    double wartosc() const{return(static_cast<double>((sklej_dwie(8)))/100.0);}
    std::string jednostka()const{return("wilgotnosc");}
};
class Ramka_cisn : public Ramka{
    public:
    Ramka_cisn(const std::vector<uint8_t>& ramka):Ramka(ramka){}
    double wartosc() const {return(static_cast<double>(sklej_cztery(8))/100.0);}
    std::string jednostka()const{return("hPa");}
};

void wypisz_i_zapisz(const std::vector<std::unique_ptr<Ramka>>& gotowe_ramki){
    std::ofstream log("log.txt");
    for(const auto&a:gotowe_ramki){
        a->wypisz(log);
        a->wypisz(std::cout);
    }
}



int main()
{
std::ifstream strumien("test.bin", std::ios::binary);
char c;
std::vector<std::uint8_t> wczytane;
while(strumien.get(c)){
    wczytane.push_back(static_cast<uint8_t>(c));
}
std::size_t i=0;
std::size_t dlugosc =0;
std::uint8_t znak = 0;
std::vector<std::unique_ptr<Ramka>> gotowe_ramki;
while( i <  wczytane.size()){
    znak = wczytane[i];
    if (znak == 0xAA && i + 7 < wczytane.size() && (wczytane[i + 7] == 2 || wczytane[i + 7] == 4) && i + 8 + wczytane[i + 7] < wczytane.size()){
        dlugosc = wczytane[i+7];
        std::uint8_t suma=0;
        for(std::size_t u=i+1; u<(i+dlugosc+8);u++){
            suma=wczytane[u]+suma;
        }
        std::vector<uint8_t> dane;
        if (suma == wczytane[i+8+dlugosc]){
                for(std::size_t u=i;u<=(i+dlugosc+8);u++){
                    dane.push_back(wczytane[u]);
                }
                switch(dane[1]){
                    case(0x01):
                        gotowe_ramki.push_back(std::make_unique<Ramka_temp>(dane));
                        i=i+9+dlugosc;
                        break;
                    case(0x02):
                        gotowe_ramki.push_back(std::make_unique<Ramka_wilg>(dane));
                        i = i+9+dlugosc;
                        break;
                    case(0x03):
                        gotowe_ramki.push_back(std::make_unique<Ramka_cisn>(dane));
                        i=i+9+dlugosc;
                        break;
                    default:
                        i=i+9+dlugosc;
                        break;
                }
                continue;
        }
        
    }
    i++;
}
    wypisz_i_zapisz(gotowe_ramki);}

