#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<vector>
#include<fstream>

#include "monitor.h"
#include<iostream>

using namespace std;

class ElevCtrl: public Monitor {
    /* variables, condition variables etc here */
    int nOfPeopleInside = 0;
    int capOfPeople = 0;
    int totalPeople = 0;
    int currentWeight = 0;
    int maxWeight = 0;
    int currentFloor = 0;
    int maxFloor = 0;  // nOfFloors - 1
    int direction = 0; //0 for idle, -1 for down, 1 for up

    int travelTime = 0;
    int idleTime = 0;
    int floorTime = 0;

    int servedPerson = 0;

    int key1 = 0; //flag to check whether elevator became idle or not
    int isFinished = 0;

    vector<int> destQueue;

    Condition willRequest;      //istek yapamadi, her idle olusunda wake et
    Condition requestedHigh;    //request yapmis ve elevator gelmesini bekliyor, her floor changede wake ediliyor, high prio
    Condition requestedLow;     //this is low prio person
    Condition inElevator;
    Condition tmdWaitCond;

    /* variables, condition variables etc here */

public:

    ElevCtrl(int nF, int nP, int wC, int pC, int TT, int IT, int IOT): willRequest(this), inElevator(this)
            , tmdWaitCond(this), requestedHigh(this), requestedLow(this) { // cv2(this)  pass "this" to cv constructors 
        nOfPeopleInside = 0;
        capOfPeople = pC;
        totalPeople = nP;
        currentWeight = 0;
        maxWeight = wC;
        currentFloor = 0;
        maxFloor = nF - 1;  // nOfFloors - 1
        direction = 0; //0 for idle, -1 for down, 1 for up

        travelTime = TT;
        idleTime = IT;
        floorTime = IOT;
    }

    ~ElevCtrl() {

    }

    void addQueue(vector<int>& q, int n, int dir){ //TODO: Duplicate element
        if (q.size()==0){
            q.push_back(n);
        }
        else {
            int place;
            int i;
            if (dir == 1){//yukari yonlu, 1,2,3
                for (i = 0; i < q.size(); i++){
                    if (q[i] == n){
                        return;
                    }
                    else if (q[i]>n){
                        break;
                    }
                }
                q.insert(q.begin()+i, n);
            }
            else {
                for (i = 0; i < q.size(); i++){//asagi yonlu, 3,2,1
                    if (q[i] == n){
                        return;
                    }
                    else if (q[i]<n){
                        break;
                    }
                }
                q.insert(q.begin()+i, n);
            }
        }
    }

    int isFinishedCtrl(){
        return isFinished;
    }


    void movesOfElevator(){
        __synchronized__;
        //cout << "Total People: " << totalPeople << endl;
        willRequest.notifyAll();
        while (servedPerson < totalPeople){
            //cout << servedPerson << " " << currentFloor << " " << totalPeople << endl;
            
            while(destQueue.size() == 0){
                tmdWaitCond.tmdWait(idleTime);               
            }
            //cout << "bi gelisme" << endl;
            //request geldi, destqda bisey var

            while (destQueue.size() > 0){
                //cout << "current floor: " << currentFloor  << " " << destQueue.size() << " " << destQueue[0] << endl;
                key1 = 0; //clear the flag
                tmdWaitCond.tmdWait(travelTime);
                currentFloor+=direction;
                
                if (currentFloor == destQueue[0]){
                    destQueue.erase(destQueue.begin());
                }
                if (destQueue.size() == 0){
                    direction = 0;  //make it idle
                    key1 = 1;  //set the flag
                }

                printElevator();

                inElevator.notifyAll();
                tmdWaitCond.tmdWait(floorTime/2);
                requestedHigh.notifyAll();
                tmdWaitCond.tmdWait(floorTime/4);
                requestedLow.notifyAll();
                tmdWaitCond.tmdWait(floorTime/4);
                
            }

            direction = 0;
            nOfPeopleInside = 0;
            if (servedPerson < totalPeople){
                willRequest.notifyAll();
                tmdWaitCond.tmdWait(idleTime);
            }
        }
        isFinished = 1;
    }

    void printPerson(int pid, int prio, int iF, int dF, int w, int j){
        string p = prio == 1 ? "hp" : "lp";
        string job;

        if      (j ==  1)   { job = ") entered the elevator";  }
        else if (j == -1)   { job = ") has left the elevator"; }
        else                { job = ") made a request";        } 

        cout << "Person ("<< pid << ", " << p << ", " << iF << " -> " << dF << ", " << w << job << endl;
    }

    void printElevator(){
        string status;
        //cout << "printing elevator| direction: " << direction << endl;
        if      (direction ==  1)   { status = "Moving-up";   }
        else if (direction == -1)   { status = "Moving-down"; }
        else                        { status = "Idle";        } 

        cout << "Elevator (" << status << ", " << currentWeight << ", " << nOfPeopleInside << ", " << currentFloor << " -> ";
        for (size_t i = 0; i < destQueue.size(); i++)
        {
            cout << destQueue[i];
            if (i < (destQueue.size()-1)){
                cout << ",";
            }
        }
        cout << ")" << endl;
    }

    void makeNewRequest(int wP, int iF, int dF, int p, int pid){
        __synchronized__;
        int dir = iF < dF ? 1 : -1;
        int entered = 0;
        int newDirElevator;

        //cout << pid << " giris yapti.." << wP << " " << iF << " " << dF << " " << p<< endl;

        while(!entered){
            willRequest.wait(); //sorun cikarabilir TODO
            while ( !((direction == 0) || ((direction == 1) && (dir == 1) && (iF>=currentFloor))  || ((direction == 0) && (dir == 0) && (iF<=currentFloor)) ) ) {
                willRequest.wait();
            }

            //TODO requesti yap, queue ekle vs.
            if      (iF < currentFloor) { newDirElevator = -1;  direction = newDirElevator; }
            else if (iF > currentFloor) { newDirElevator =  1;  direction = newDirElevator; }
            else                        { newDirElevator =  0;  }

            //cout << "person| direction: " << direction<< " " <<  iF << " " << currentFloor << endl;
            

            if      (newDirElevator!=0) { addQueue(destQueue,iF,newDirElevator); } 

            printPerson(pid, p, iF, dF, wP, 0);
            printElevator();

            if (currentFloor == iF){
                //look for entrance conditions
                if ( (direction * dir == -1) || (capOfPeople <= nOfPeopleInside) || (wP > (maxWeight - currentWeight))){
                    entered = 0; //elevatora giremedi, tekrar istek atmak icin loopu basa sar
                    continue;
                }
                else {
                    direction = dir;
                    nOfPeopleInside++;
                    currentWeight+=wP;
                    addQueue(destQueue,dF,direction);
                    entered=1;
                    printPerson(pid, p, iF, dF, wP, 1);
                    printElevator();
                    break; //elavatora girdi, looptan cik, elevatorda bekle.
                }
            }
            else {  //ilk istegini attiktan sonra, asansorun gelmesini bekliyor
                while (currentFloor != iF){
                    if (p == 1) {  requestedHigh.wait();    }
                    else        {  requestedLow.wait();     }
                }
                //elevator seninle ayni kata geldi, dest queue kontrol et 
                //baska biri senden once istek atmis olabilir

                if (destQueue.size() == 0 ){
                    //doRequest and print things..
                    printPerson(pid, p, iF, dF, wP, 0);
                    printElevator();

                    direction = dir;
                    nOfPeopleInside++;
                    currentWeight+=wP;
                    addQueue(destQueue,dF,direction);
                    entered = 1;
                    printPerson(pid, p, iF, dF, wP, 1);
                    printElevator();
                    break;
                    //destqueue bos, request yap, enter et gir ve return et
                }
                else {
                    //baska biri daha once istek atmis, direction i kontrol et
                    if (direction == dir){ // ayni yon, entrance conditions bak
                        if ( (capOfPeople > nOfPeopleInside) && (wP <= (maxWeight - currentWeight))){//girer
                            
                            if (key1){ //elevator became idle once, so make request
                                printPerson(pid, p, iF, dF, wP, 0);
                                printElevator();
                            }

                            nOfPeopleInside++;
                            currentWeight+=wP;
                            addQueue(destQueue,dF,direction);
                            entered = 1;
                            printPerson(pid, p, iF, dF, wP, 1);
                            printElevator();
                            break;
                        }
                        else {  //giremez, tekrar istek atmayi bekler
                            entered = 0;
                            continue;
                        }
                        //ayni yon, queue ekle kendini, enter et ve return et, request yapilicak mi bilinmiyor.
                    }
                    else {
                        entered = 0;
                        continue;
                        //farkli yondesin, giremedin tekrar bekle.
                    }

                }
            }
        }

        while(currentFloor != dF){
            inElevator.wait();
            //elevator destinatione gelene kadar loopla, her floor changede uyandirilicaksin
        }
        
        nOfPeopleInside--;
        currentWeight-=wP;
        servedPerson++;
        printPerson(pid, p, iF, dF, wP, -1);
        printElevator();
        //do printing here..

        //destinationa geldin, cikis yap

        
    }
};

struct PParam {
    ElevCtrl *ec;
    int wP;
    int iF;
    int dF;
    int p;
    int pid;
};

void *elevatorFunc(void *p){
    ElevCtrl * ec = (ElevCtrl *) p;
    //cout << "sea" << endl;
    ec->movesOfElevator();
}

void *personFunc(void *p){
    PParam * prm = (PParam *) p;
    ElevCtrl * ec = prm->ec;

    //cout <<"monitor disi func| " << prm->wP << " " << prm-> iF << " " << prm->dF << " " << prm->p << endl;
    int pid = prm->pid;
    int wP = prm->wP;
    int iF = prm->iF;
    int dF = prm->dF;
    int pr  = prm->p;

    //cout << "ase" <<endl;
    ec->makeNewRequest(wP,iF,dF,pr,pid);
}

int main(int argc, char *argv[]){
    pthread_t *persons, elevatorControl;
    
    vector<int> weights;
    vector<int> initials;
    vector<int> destinations;
    vector<int> priorities;

    string fileToRead;

    //cout << argc << endl;
    //cout << argv[0] << endl;
    fileToRead = argv[1];

    ifstream readObj(fileToRead);

    int nOfFloors;
    int nOfPeople;
    int weight;
    int pCap;
    int travel;
    int idle;
    int iot;

    int w,i,d,p;

    readObj >> nOfFloors >> nOfPeople >> weight >> pCap >> travel >> idle >> iot;

    //cout << "Main| nOfPeople: " << nOfPeople <<endl;

    for (size_t jj = 0; jj < nOfPeople; jj++)
    {
        readObj >> w >> i >> d >> p;
        weights.push_back(w);
        initials.push_back(i);
        destinations.push_back(d);
        priorities.push_back(p);
    }

    //cout << nOfFloors << nOfPeople << weight << pCap << travel << idle << iot << endl;

     PParam * pparams = new PParam[nOfPeople];
    ElevCtrl ec(nOfFloors, nOfPeople, weight, pCap, travel, idle, iot);

    persons = new pthread_t[nOfPeople];

    pthread_create(&elevatorControl, NULL, elevatorFunc, (void *) &ec);

    for (size_t i = 0; i < nOfPeople; i++)
    {
        pparams[i].pid = i;
        pparams[i].ec = &ec;
        pparams[i].wP = weights[i];
        pparams[i].iF = initials[i];
        pparams[i].dF = destinations[i];
        pparams[i].p  = priorities[i];

        pthread_create(&persons[i], NULL, personFunc, (void *) (pparams + i));
    }
    
    while(!ec.isFinishedCtrl()){
        usleep(10000);
    }

    for (size_t i = 0; i < nOfPeople ; i++)
    {
        pthread_join(persons[i],NULL);
    }

    pthread_join(elevatorControl, NULL);
    

    return 0;
}