
template<typename T> class cudaVector
{
public:
    typedef T typevalue;
    __device__ int size();
    __device__ void push_back(T val);
    __device__ cudaVector<T>();
    __device__ ~cudaVector<T>();
    __device__ T& operator[] (int n);

private:
    T* arr;
    int len, current_length, scale;
};