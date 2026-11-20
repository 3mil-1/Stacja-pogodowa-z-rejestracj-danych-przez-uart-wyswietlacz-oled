#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
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
    if (znak == 0xAA){
        dlugosc = wczytane[i+7];
        std::uint8_t suma=0;
        for(std::size_t u=i; u<(i+dlugosc+8);u++){
            suma=wczytane[u]+suma;
        }
        
    }
}    
}