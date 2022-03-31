#include <iostream>

int main(int argc,char* argv[])
{
    int num1,num2;
   
   printf("first number : ");
    std::cin >> num1;


    printf("second number : ");
    std::cin >> num2;

    printf("%d+%d= %d\n",num1,num2,num1+num2);
    printf("%d-%d= %d\n",num1,num2,num1-num2);
    printf("%d*%d= %d\n",num1,num2,num1*num2);
    printf("%d/%d= %d\n",num1,num2,num1/num2);
}
