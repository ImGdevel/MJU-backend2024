#include <iostream>
#include <thread>
#include <chrono> 

using namespace std;

void f1(){
    cout << "f1: " << this_thread::get_id() << endl;
    this_thread::sleep_for(chrono::milliseconds(60*1000));
    cout << "f1: work up" << endl;
}

void f2(int msg){
    cout << "f2: " << msg << endl;
}

int main(){
    thread t1;
    thread t2(f1);
    thread t3(f2,10);

    cout << "C++ id: " << t3.get_id() << endl;
    cout << "Native id: " << t3.native_handle() << endl;
    
    t2.join();
    t3.join();

    return 0;
}