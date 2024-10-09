#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <fstream>

#include "person.pb.h"

#include <iostream>
#include <string>

using namespace std;
using namespace std;
using namespace mju;


int main(){
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(s <0) return 1;

    string buf = "Hello World";
    struct sockaddr_in sin;
    memset(&sin, 0 , sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(10001);
    sin.sin_addr.s_addr = inet_addr("127.0.0.1");


    Person *p = new Person;
    p->set_name("MJ Kim");
    p->set_id(12345678);

    Person::PhoneNumber* phone = p->add_phones();
    phone->set_number("010-111-1234");
    phone->set_type(Person::MOBILE);

    phone = p->add_phones();
    phone->set_number("02-100-1000");
    phone->set_type(Person::HOME);

    string proto = p->SerializeAsString();

    int numBytes = sendto(s, proto.c_str(), proto.length(), 0, (struct sockaddr *) &sin, sizeof(sin));
    cout << "Sent: " << numBytes << endl;

    char buf2[65536];
    memset(&sin, 0, sizeof(sin));
    socklen_t sin_size = sizeof(sin);
    numBytes = recvfrom(s, buf2, sizeof(buf2), 0, (struct sockaddr *)&sin, &sin_size);

    cout << "Recevied: " << numBytes << endl;
    cout << "From: " << inet_ntoa(sin.sin_addr) << endl;

    string receivedData(buf2, numBytes);
    Person *p2 = new Person;
    p2->ParseFromString(receivedData);
    
    cout << "Name: " << p2->name() << endl;
    cout << "ID: " << p2->id() << endl;
    for(int i=0; i < p2->phones_size(); i++){
        cout << "Type: " << p2->phones(i).type() << endl;
        cout << "Phone: " << p2->phones(i).number() << endl;
    }

    close(s);
    return 0;
}


