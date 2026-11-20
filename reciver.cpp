#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>

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
};
class Ramka_temp : public Ramka  {
    public:
     Ramka_temp (const std::vector<uint8_t>& ramka):Ramka(ramka){}
    double wartosc()const{return()}

};




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

while( i <  wczytane.size()){
    znak = wczytane[i];
    if (znak == 0xAA && wczytane[i+7]){
        dlugosc = wczytane[i+7];
        std::uint8_t suma=0;
        for(std::size_t u=i; u<(i+dlugosc+8);u++){
            suma=wczytane[u]+suma;
        }
        if (suma = wczytane[i+8+dlugosc]){
            
        }
        
    }
}    