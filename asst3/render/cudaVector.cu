#include "cudaVector.h"

template<typename T>
__device__ cudaVector<T>::cudaVector()
{
    len = 0;
    current_length = 2;
    scale = 2;
    cudaMalloc(&arr, sizeof(T) * current_length);
}

template<typename T>
__device__ cudaVector<T>::~cudaVector()
{
    cudaFree(arr);
}

template<typename T>
__device__ int cudaVector<T>::size()
{
    return len;
}

template<typename T>
__device__ void push_back(T val)
{
    if (len == current_length)
    {
        T* tmp = arr;
        current_length *= scale;
        cudaMalloc(&arr, sizeof(T) * current_length);
        for (int i = 0; i < len; i++)
        {
            arr[i] = tmp[i];
        }
        cudaFree(tmp);
    }
    arr[len++] = val;
}

template<typename T>
__device__ T& cudaVector<T>::operator[] (int n)
{
    return arr[n];
}