// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <string>

using namespace std;

int main() {

    int result = 0;

    for (int i = 0; i <= 1000; ++i) {
        string str = to_string(i);
        for (char c : str) {
            if (c == '3') {
                ++result;
                break;
            }
        }
    }
    cout << result << endl;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
