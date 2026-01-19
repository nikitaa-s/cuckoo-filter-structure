#include <iostream>
#include <fstream>
#include <map>
#include <random>
#include <ctime>
#include <string>

using namespace std;

class Filter {
private:
    // размер фингерпринта
    static const int FINGER_PRINT_SIZE = 7;
    typedef uint8_t fp_t;
    // каждый бакет это массив из 4 uint8_t
    typedef uint8_t Bucket[4];
    typedef uint64_t hash_t;
    uint64_t countBuckets;
    const int MAX_NUM_KICKS = 500;
public:
    Bucket *masBuckets;

    explicit Filter(int countBuckets) {
        // количество бакетов округление в большую сторону к степени двойки
        this->countBuckets = roundToPowerOfTwo(ceil(countBuckets * 1.06));
        masBuckets = new Bucket[this->countBuckets];
        // зануляем, чтобы не было мусора
        for (int i = 0; i < this->countBuckets; ++i) {
            for (int j = 0; j < 4; ++j) {
                masBuckets[i][j] = 0;
            }
        }
    }

    explicit Filter() {
        this->countBuckets = 0;
        masBuckets = new Bucket[this->countBuckets];
    }

    // освобождение ресурсов (удаление)
    ~Filter() {
        delete[] masBuckets;
    }

/*
 * Хеш функция для фингерпринта
 */
    static fp_t hashFingerPrint(const char *str) {
        size_t hash = 5381;
        while (uint8_t c = *str++) {
            hash = ((hash << 5u) + hash) + c;
        }
        return hash % ((1u << unsigned(FINGER_PRINT_SIZE)) - 1) + 1;
    }

/*
 * Хеш функция для определения индекса бакета
 */
    hash_t hashBucket(const char *str) const {
        return std::hash<string>{}(str) % countBuckets;

    }

/*
 * вставка в фильтр и выталкивание из него
 */
    void add(const char *str) const {
        // фингерпринт от строки
        uint8_t x = hashFingerPrint(str);
        // индекс 1 бакета
        uint64_t i = hashBucket(str);
        // индекс 2 бакета
        uint64_t j = (i ^ hashBucket(to_string(hashFingerPrint(str)).c_str())) % countBuckets;
        // цикл огрниченный на количество выталкиваний, чтобы избежать зацикливания
        for (int count = 0; count < MAX_NUM_KICKS; ++count) {
            // ходим по свободным ячейкам бакета
            for (int k = 0; k < 4; ++k) {
                // если ячейка свободна в бакете по i индексу, то кладем фингерпринт
                if (masBuckets[i][k] == 0) {
                    masBuckets[i][k] = x;
                    return;
                }
                // если ячейка свободна в бакете по j индексу, то кладем фингерпринт
                if (masBuckets[j][k] == 0) {
                    masBuckets[j][k] = x;
                    return;
                }
                // дошли до k=3 значит не нашлось свободного места, значит будем выталкивать
                if (k == 3) {
                    // Для различных рандомных значений в заданных диапазонах
                    std::random_device rand;
                    std::uniform_int_distribution<int> UID(0, 1);
                    std::uniform_int_distribution<int> UID2(0, 3);
                    int r = UID(rand);
                    uint64_t p;
                    // p - индекс бакета из которого хотим вытолкнуть (определяется рандомно)
                    if (r == 0) p = i;
                    else p = j;
                    // индекс фингерпринта который хотим вытолкнуть
                    int indexFingerPrint = UID2(rand) % 4;
                    // элемент который выталкиваем
                    uint8_t e = masBuckets[k][indexFingerPrint];
                    // то что изначально хотели записать
                    masBuckets[k][indexFingerPrint] = x;
                    // находим второй возможный индекс бакета для элемента e который вытолкнули
                    p = p ^ hashBucket(to_string(e).c_str());
                    // теперь будем ходить только по бакету с таким индексом и пытаться записать элемент
                    // если не получится повторим и так до зацикливания пока не получится найти пустое место
                    i = j = p;
                    // элемент который записываем
                    x = e;
                }
            }
        }
    }

/*
 * проврека посмотрел ли пользователь видео
 */
    string check(const char *str) const {
        // фингерпринт
        uint8_t x = hashFingerPrint(str);
        // 1 бакет
        uint64_t i = hashBucket(str);
        // 2 бакет
        uint64_t j = (i ^ hashBucket(to_string(hashFingerPrint(str)).c_str())) % countBuckets;
        for (int k = 0; k < 4; ++k) {
            // если хотя бы в одном из бакетов лежит элемент то скорее всего смотрел иначе точно не смотрел
            if (masBuckets[i][k] == x || masBuckets[j][k] == x) return "Probably";
        }
        return "No";
    }

/*
 * Округление в сторону ближайшей степени двойки
 */
    static uint64_t roundToPowerOfTwo(uint64_t size) {
        uint64_t k;
        uint64_t l;

        for (k = size; (l = k & (k - 1)) != 0; k = l);

        return (size + (k - 1)) & (~(k - 1));
    }


};

// словарь пользователей и фильтра для них
static map<string, Filter *> mapUsers;

// Чтение из файла .
static string fillInfoFromFile(const std::string &pathInput) {
    fstream file(pathInput);
    string way, user, video;
    uint64_t countVideo;
    string finalRes = "Ok";
    // прочитали для понимания количества бакетов
    file >> video >> countVideo;
    // чтение файла
    while (!file.eof()) {
        file >> way >> user >> video;
        if (way == "check") {
            // нет такого пользователя в словаре
            if (mapUsers.count(user) == 0) {
                finalRes += "\nNo";
                // проверяем по полученному пользователю и видео смотрел ли он его
            } else {
                string res = mapUsers[user]->check(video.c_str());
                finalRes += "\n" + res;
            }

        } else {
            // если пользователь уже есть добавляем видео в фильтр
            if (mapUsers.count(user) > 0) {
                mapUsers[user]->add(video.c_str());
                // пользователя нет значит создаем нового, создаем фильтр и добавляем туда видео
            } else {
                mapUsers.insert(make_pair(user, new Filter(countVideo)));
                mapUsers[user]->add(video.c_str());
            }
            finalRes += "\nOk";
        }


    }
    return finalRes;
}

static void printFile(const string &res, const string &pathPrint) {
    std::ofstream out; // поток для записи
    out.open(pathPrint); // окрываем файл для записи
    if (out.is_open()) {
        out << res;
    }
}

int main(int argc, char *argv[]) {
    if (argc == 3) {
        string path = argv[1];
        string res = fillInfoFromFile(path);
        // очистка
        for (const auto &elem:mapUsers) {
            delete elem.second;
        }
        printFile(res, argv[2]);
    }
    return 0;
}
