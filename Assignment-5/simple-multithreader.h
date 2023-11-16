#include <iostream>
#include <list>
#include <functional>
#include <stdlib.h>
#include <cstring>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

int user_main(int argc, char **argv);

/* Demonstration on how to pass lambda as parameter.
 * "&&" means r-value reference. You may read about it online.
 */
void demonstration(std::function<void()> && lambda) {
  lambda();
}

int main(int argc, char **argv) {
  /* 
   * Declaration of a sample C++ lambda function
   * that captures variable 'x' by value and 'y'
   * by reference. Global variables are by default
   * captured by reference and are not to be supplied
   * in the capture list. Only local variables must be 
   * explicity captured if they are used inside lambda.
   */
  int x=5,y=1;
  // Declaring a lambda expression that accepts void type parameter
  auto /*name*/ lambda1 = /*capture list*/[/*by value*/ x, /*by reference*/ &y](void) {
    /* Any changes to 'x' will throw compilation error as x is captured by value */
    y = 5;
    std::cout<<"====== Welcome to Assignment-"<<y<<" of the CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  // Executing the lambda function
  demonstration(lambda1); // the value of x is still 5, but the value of y is now 5

  int rc = user_main(argc, argv);

  auto /*name*/ lambda2 = [/*nothing captured*/]() {
    std::cout<<"====== Hope you enjoyed CSE231(A) ======\n";
    /* you can have any number of statements inside this lambda body */
  };
  demonstration(lambda2);
  return rc;
}

// Variable for Storing Arguments of Each Thread
// Total 4 arguments - low,high
int arguments_arr_vector[10000000][2];
// Variable for Storing Arguments of Each Thread
// Total 4 arguments - low,high,rows,columns
int arguments_arr_matrix[10000000][4];

struct ThreadArgs_vector {
    int thread_no;
    std::function<void(int)> lambda;
};

struct ThreadArgs_matrix {
    int thread_no;
    std::function<void(int,int)> lambda;
};

void *vector_sum_func(void *args) {
    ThreadArgs_vector *thread_args = static_cast<ThreadArgs_vector *>(args);
    for (int index = arguments_arr_vector[thread_args->thread_no][0]; index <= arguments_arr_vector[thread_args->thread_no][1]; index++) {
        thread_args->lambda(index);
    }
    return NULL;
}

void parallel_for(int low, int high, std::function<void(int)> &&lambda, int numThreads) {
  clock_t func_start = clock();
  pthread_t id[numThreads];
  int sub_arr_size = (high-low)/numThreads;

  for (int i = 0; i < numThreads; i++) {
    if(i==0){
      arguments_arr_vector[i][0] = i*sub_arr_size;
    }
    else{
      arguments_arr_vector[i][0] = i*sub_arr_size+1;
    }
    arguments_arr_vector[i][1] = (i+1)*sub_arr_size;

    // Allocate memory for thread arguments
    ThreadArgs_vector *thread_args = new ThreadArgs_vector{i,lambda};
    int creation_check = pthread_create(&id[i], NULL, &vector_sum_func, thread_args);
    //Error Checking
    if(creation_check != 0){
      printf("Error Occured While Creating Threads\n");
      exit(EXIT_FAILURE);
    }
  }

  for (int j = 0; j < numThreads; j++) {
      pthread_join(id[j], NULL);
  }

  clock_t func_end = clock();
  double time_taken = ((double)(func_end - func_start))/CLOCKS_PER_SEC;
  printf("Total Execution Time - %f seconds\n",time_taken);
}

#define main user_main
// running command - g++ -std=c++11 -o vector vector.cpp && ./vector

void *matrix_sum_func(void *args) {
    ThreadArgs_matrix *thread_args = static_cast<ThreadArgs_matrix *>(args);
    int row_size = arguments_arr_matrix[thread_args->thread_no][2];
    int col_size = arguments_arr_matrix[thread_args->thread_no][3];
    for (int index = arguments_arr_matrix[thread_args->thread_no][0]; index <= arguments_arr_matrix[thread_args->thread_no][1]; index++) {
      if((index+1)%col_size == 0){
        thread_args->lambda(((index+1)/row_size) - 1,col_size-1);
      }
      else{
        thread_args->lambda((index+1)/row_size,((index+1)%col_size)-1);
      }
    }
    return NULL;
}

void parallel_for(int low1, int high1,int low2, int high2, std::function<void(int,int)> &&lambda, int numThreads) {
  clock_t func_start = clock();
  pthread_t id[numThreads];
  int total_entries = (high2-low2)*(high1-low1);
  int row_size = (high1-low1);
  int col_size = (high2-low2);
  int sub_arr_size = total_entries/numThreads;

  for (int i = 0; i < numThreads; i++) {
    if(i==0){
      arguments_arr_matrix[i][0] = i*sub_arr_size;
    }
    else{
      arguments_arr_matrix[i][0] = i*sub_arr_size+1;
    }
    arguments_arr_matrix[i][1] = (i+1)*sub_arr_size;
    arguments_arr_matrix[i][2] = row_size;
    arguments_arr_matrix[i][3] = col_size;

    // Allocate memory for thread arguments
    ThreadArgs_matrix *thread_args = new ThreadArgs_matrix{i,lambda};
    int creation_check = pthread_create(&id[i], NULL, &matrix_sum_func, thread_args);
    //Error Checking
    if(creation_check != 0){
      printf("Error Occured While Creating Threads\n");
      exit(EXIT_FAILURE);
    }

  }

  for (int j = 0; j < numThreads; j++) {
      pthread_join(id[j], NULL);
  }
  clock_t func_end = clock();
  double time_taken = ((double)(func_end - func_start))/CLOCKS_PER_SEC;
  printf("Total Execution Time - %f seconds\n",time_taken);
}