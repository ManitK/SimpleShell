#include <stdio.h> // Function to calculate Fibonacci number recursively 
int fibonacci(int n) { 
    if (n <= 1) { 
        return n; 
    } 
    else { 
        return fibonacci(n - 1) + fibonacci(n - 2); 
        } 
    }

int main(){
    printf("%d \n",fibonacci(45));
}

