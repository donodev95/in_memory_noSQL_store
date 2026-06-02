#include <iostream>
int main(int argc, char const *argv[])
{
    std::array<char, 5> test {'h', 'e', 'l', 'l', 'o'};
    std::cout << &test << std::endl;
    std::string_view test1 {"Hello Dono"};
    std::cout << test1.data() << std::endl;

    
    return 0;
}
