#include <iostream>
int main(int argc, char const *argv[])
{
    std::array<char, 5> test {'h', 'e', 'l', 'l', 'o'};
    std::cout << &test << std::endl;
    std::string_view test1 {"Hello Dono"};
    std::cout << test1.data() << std::endl;
    char test_char[3]{'a', '2', 'b'};
    std::cout << test_char << std::endl; 
    std::array<char, 10> read{"h"};
    std::cout << "TEST buffer" << read.size() << std::endl;
    return 0;
}
