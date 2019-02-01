#include <iostream>
#include <thread>
#include <cstring>
#include <condition_variable>

// реализация семафора
class Semaphore {
public:
    explicit Semaphore(const size_t count = 0) : count_(count) {}
    void Acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (count_ == 0) {
            has_tokens_.wait(lock);
        }
        count_--;
        if (count_ < min_count_) {
            min_count_ = count_;
        }
    }
    void Release() {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        has_tokens_.notify_one();
    }
    size_t getMaxThreadsNumber() { return count_ - min_count_; }
    void setCount(size_t count) {
        count_ = count;
        min_count_ = count;
    }
private:
    size_t count_;
    size_t min_count_ = count_;
    std::condition_variable has_tokens_;
    std::mutex mutex_;
};

Semaphore semaphore;

// проверка корректности работы сортировки
void checkCorrect(const double *array, size_t size) {
    for (size_t i = 0; i < size-1; ++i) {
        if (array[i] > array[i+1]) {
            printf("sorting is incorrect");
            return;
        }
    }
    printf("sorted\n");
}

// компаратор для qsort
int comp(const void * a, const void * b) {
    return ( *(double*)a - *(double*)b );
}

// сортировка выбором (запускается в однопоточной сортировке слиянием
// при малом числе элементов массива)
void InsertionSort(double *array, size_t n) {
    for (size_t i = 0; i < n - 1; ++i) {
        size_t k = i;
        for (size_t j = i + 1; j < n; ++j) {
            if (array[j] < array[k]) {
                k = j;
            }
        }
        std::swap(array[i], array[k]);
    }
}

// бинарный поиск (требуется для многопоточного слияния массивов)
size_t BinarySearch(double x, const double *array, size_t left, size_t right) {
    if ((right <= left) || (x <= array[left])) {
        return left;
    }
    size_t mid = (left+right)/2;
    while (!((array[mid-1] < x) && (array[mid] >= x))) {
        if (array[mid] < x) {
            left = mid;
        } else {
            right = mid;
        }
        mid = (left+right)/2;
    }
    return mid;
}

// функция слияния в один поток
void SingleThreadedMerge(const double *a, size_t aLen, const double *b, size_t bLen, double *c) {
    size_t i = 0, j = 0;
    while (i < aLen && j < bLen) {
        if (a[i] < b[j]) {
            c[i + j] = a[i];
            ++i;
        } else {
            c[i + j] = b[j];
            ++j;
        }
    }
    if (i == aLen) {
        for (; j < bLen; ++j) {
            c[i + j] = b[j];
        }
    } else {
        for (; i < aLen; ++i) {
            c[i + j] = a[i];
        }
    }
}

// однопоточная сортировка слиянием
void SingleThreadedMergeSort(double *array, double *c, size_t aLen) {
    if (aLen <= 10) {
        InsertionSort(array, aLen);
        return;
    }
    size_t Len1 = aLen / 2;
    size_t Len2 = aLen - Len1;
    SingleThreadedMergeSort(array, c, Len1);
    SingleThreadedMergeSort(array + Len1, c + Len1, Len2);
    SingleThreadedMerge(array, Len1, array + Len1, Len2, c);
    memcpy(array, c, sizeof(double)*aLen);
}

// функция многопоточного слияния
void MultiThreadedMerge(const double *a, size_t aLen, const double *b, size_t bLen, double *c) {
    semaphore.Acquire();
    if (aLen < bLen) {
        std::swap(a, b);
        std::swap(aLen, bLen);
    }
    if (bLen <= 500000) {
        SingleThreadedMerge(a, aLen, b, bLen, c);
        semaphore.Release();
        return;
    }
    size_t mid1 = aLen/2;
    size_t mid2 = BinarySearch(a[mid1], b, 0, bLen-1);
    size_t mid3 = mid1 + mid2;
    c[mid3] = a[mid1];

    std::thread thread1(MultiThreadedMerge, a, mid1, b, mid2, c);
    std::thread thread2(MultiThreadedMerge, a+mid1+1, aLen-mid1-1, b+mid2, bLen-mid2, c+mid3+1);
    semaphore.Release();
    thread1.join();
    thread2.join();
}

// многопоточная сортировка слиянием
void MultiThreadedMergeSort(double *array, double *c, size_t aLen) {
    semaphore.Acquire();
    if (aLen <= 1000000) {
        qsort(array, aLen, sizeof(double), comp);
        semaphore.Release();
        return;
    }
    size_t Len1 = aLen / 2;
    size_t Len2 = aLen - Len1;

    std::thread thread1(MultiThreadedMergeSort, array, c, Len1);
    std::thread thread2(MultiThreadedMergeSort, array+Len1, c+Len1, Len2);
    semaphore.Release();
    thread1.join();
    thread2.join();

    MultiThreadedMerge(array, Len1, array + Len1, Len2, c);
    memcpy(array, c, sizeof(double)*aLen);
}

int main(int argc, const char * argv[]) {
    size_t size = atoll(argv[2]);
    semaphore.setCount(atoll(argv[1]));
    double *array = (double *) malloc(sizeof(double) * size);
    for (size_t i = 0; i < size; ++i) {
        array[i] = rand();
    }
    double *c = (double *) malloc(sizeof(double) * size);
    time_t start_time = time(nullptr);

    MultiThreadedMergeSort(array, c, size);

    time_t end_time = time(nullptr);
    checkCorrect(array, size);
    printf("Time : %ld\n", end_time - start_time);
    printf("Maximum number of concurrent threads : %ld\n", semaphore.getMaxThreadsNumber());
    free(array);
    free(c);
    return 0;
}
