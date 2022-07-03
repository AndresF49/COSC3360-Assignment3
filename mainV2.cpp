#include <iostream>
#include <string>
#include <unistd.h>
#include <map>
#include <iterator>
#include <vector>
#include <math.h>
#include <pthread.h>

using namespace std;

// create a struct that has integer value, numBits, pointer to the compressed message
struct Data {
    int base10Val;
    char letter;
};

struct Data2 {
    int * turn;
    int loc;
    string binary;
    char letter;
    int numBits;
    int frequency;
    string message;
    pthread_mutex_t * bsem;
    pthread_cond_t * waitTurn;
    int lenOfDecodedMes;
    vector<Data> * ptrVecData;
    std::map<std::string, char> * alphabet;
    
};

struct Decompress2 {
    std::map<std::string,char> * alphabet;
    pthread_mutex_t * bsem;
    pthread_cond_t * waitTurn;
    std::string message;
    int numBits;
    int loc; // used as a location to start at 
    int turn; // whos turn it is
    std::string * decompressed;
};

string toBinary(int value, int numBits) {
    string bstr = "", bstr2 = "";
    int bNums[numBits];
    int remainder = 0;
    int j = 0;
    while (value != 0) {
        remainder = value % 2;
        value /= 2;
        bNums[j] = remainder;
        j++;
    }

    for (int i = j-1; i >= 0; i--) {
        bstr.append(to_string(bNums[i]));
    }

    if (bstr.length() < numBits) {
        bstr2.append(numBits - (int)bstr.length(), '0');
        bstr = bstr2 + bstr;
    } 
    return bstr;
}

int getFrequency(string finding, string &mes) {
    int amt = 0;
    while (mes != "") {
        // cout << tempMes << endl;
        if (mes.substr(0, finding.size()) == finding) {
            amt++;
        }
        mes.erase(0, finding.size());
    }
    return amt;
}

void * binaryAndFrequency(void* data) {
    // in this CS, we grab local variables at the location we want (starting at 0) then increment location
    // so that the next thread can grab the local variables at THAT location (next one)
    Data2* structData = (Data2 *) data;
    int loc = structData->loc;
    char let = structData->ptrVecData->at(loc).letter;
    int val = structData->ptrVecData->at(loc).base10Val;
    int bits = structData->numBits;
    int bNums[bits];
    string mes = structData->message;
    int amt = 0;

    structData->loc = structData->loc + 1; // increment location to start next thread
    // std::cout << "About to unlock\n";
    pthread_mutex_unlock(structData->bsem); // we are gonna start at turn = 0 and increment after we are done

    // compute binary and frequency outside of critical section //
    
    string bstr = toBinary(val, bits);
    amt = getFrequency(bstr, mes);

    // std::cout << "This is what is gonna get inserted into map: (" << bstr << "," << let << ")\n";
    
    pthread_mutex_lock(structData->bsem);
    structData->lenOfDecodedMes += amt; // adjust overall amount of times a character has shown up in our compressed mes
    
    while (*(structData->turn) != loc) { // if your turn != loc we wait until it is your turn
        // std::cout << "waiting\n";
        pthread_cond_wait(structData->waitTurn, structData->bsem);
    }
    pthread_mutex_unlock(structData->bsem);

    structData->alphabet->insert(std::pair<std::string,char>(bstr,let)); // if it is your turn we insert 
    // the binary rep and letter into map and print out the char, bin str, and freq
    std::cout << "Character: " << let << ", Code: " << bstr << ", Frequency: " << amt << std::endl;

    pthread_mutex_lock(structData->bsem); // lock the thread and increment turn so that the next thread can pass the 
    // while loop
    *(structData->turn) = *(structData->turn) + 1;
    pthread_cond_broadcast(structData->waitTurn); // wake up other threads
    pthread_mutex_unlock(structData->bsem);

    return nullptr;
}

void * decompress(void* structWithMap) {

    Decompress2* decom = (Decompress2*) structWithMap;
    // std::cout << "In decompress\n";
    // *(decom->loc) * decom->numBits will start at the next section of the string by numBits amout and will be numBits long    
    string mes = decom->message.substr(decom->loc * decom->numBits, decom->numBits);
    string * decompressPtr = decom->decompressed; // pointer that we will append to when we get our letter
    // int turn = decom->turn; 
    int loc = decom->loc; // we store the original int that loc had so that we know where we started 
    // and what we are looking for when we compare it to turn

    decom->loc = decom->loc + 1; // increment the starting location for the next thread
    pthread_mutex_unlock(decom->bsem);

    // std::cout << "Left CS\n";
    std::map<std::string,char>::iterator it;
    it = decom->alphabet->find(mes);
    char letter = it->second;

    pthread_mutex_lock(decom->bsem);
    while(loc !=  decom->turn ) { // wait if your original starting location is not the current turn
        // std::cout << "waiting\n";
        pthread_cond_wait(decom->waitTurn, decom->bsem);
    }
    // if it is the current turn, then we do this 
    *(decompressPtr) = *(decompressPtr) + letter; // add letter to string
    
    decom->turn++; // increment turn for other threads
    pthread_cond_broadcast(decom->waitTurn);
    pthread_mutex_unlock(decom->bsem);

    return nullptr;
}

int main() {
    string line;
    string n_str; // n lines and n number of symbols in this alphabet
    int n, maxNum = 0;
    vector<Data> vecData; // VECTOR OF STRUCT
    string message; // declare compressed message variable 
    struct Data2 data2;
    int turn = 0;
    data2.turn = &turn;
    data2.lenOfDecodedMes = 0;
    data2.loc = 0;

    getline(cin, n_str);
    n = stoi(n_str);

    // get all letters and their code value //
    for (int k = 0; k < n; k++) {
        // getline(input,line);
        getline(cin,line); // this is input redirection

        // VECTOR
        Data temp;
        temp.base10Val = stoi(line.substr(2));
        temp.letter = line[0];
        vecData.push_back(temp);

        if (maxNum < stoi(line.substr(2))) { // get the max code value
            maxNum = stoi(line.substr(2));
        }
    } 
    // after vec is filled with character and their base10 value, have our Data2 "store" this vector by poiting to it for access 
    // in threads
    data2.ptrVecData = &vecData;
    // get last string which is the compressed message //

    getline(cin,line); // this is input redirection
    message = line; // message variable defined
    data2.message = message;

    // calculate the # of bits used per symbol //
    int numBits = (int)ceil(log2(maxNum + 1));
    data2.numBits = numBits;

    map<string,char> *alphabet = new map<string,char>;
    data2.alphabet = alphabet;

    // lets do the tasks

    // threads will convert the value to binary then count the # of occurences in the message
    static pthread_mutex_t bsem;
    static pthread_cond_t waitTurn;

    pthread_mutex_init(&bsem, NULL);
    pthread_cond_init(&waitTurn, NULL);
    data2.bsem = &bsem;
    data2.waitTurn = &waitTurn;

    // make array to hold pthreads // make it a dynamic or static array instead //
    pthread_t* pthread = new pthread_t[n];

    std::cout << "Alphabet:\n";

    for (int i = 0; i < n; i++) {

        pthread_mutex_lock(&bsem); // lock to assign local vars
        if (pthread_create(&pthread[i], nullptr, binaryAndFrequency, &data2)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }
    for (int i = 0; i < n; i++) {
            pthread_join(pthread[i], nullptr);
    }
    // stored ^^ char and binary string into map for next set of threads and 2nd struct //
    int lenOfDecMes = data2.lenOfDecodedMes;

    // std::cout << "END OF FIRST PTHREAD FUNCT\n";

    pthread_t *pthread2 = new pthread_t[lenOfDecMes];

    Decompress2 decompress2;
    decompress2.alphabet = alphabet;
    decompress2.message = message;
    decompress2.numBits = numBits;
    decompress2.turn = 0;
    decompress2.loc = 0;
    decompress2.bsem = &bsem;
    decompress2.waitTurn = &waitTurn;
    std::string decompressedMes = "";
    decompress2.decompressed = &decompressedMes;
    
    pthread_mutex_init(decompress2.bsem, NULL);
    pthread_cond_init(decompress2.waitTurn, NULL);

    std::cout << "\nDecompressed message: ";
    // create lenOfDecMes threads to determine each character in the encoded message
    for (int i = 0; i < lenOfDecMes; i++) {

        pthread_mutex_lock(&bsem); // lock current so that we can assign local vars
        if (pthread_create(&pthread2[i], nullptr, decompress, &decompress2)) {
            fprintf(stderr, "Error creating thread\n");
            return 1;
        }
    }

    for (int i = 0; i < lenOfDecMes; i++) {
        pthread_join(pthread2[i], nullptr);
    }
    std::cout << decompressedMes << endl;

    delete alphabet;
    delete[] pthread;
    delete[] pthread2;
    return 0;
}