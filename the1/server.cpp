#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>
#include <vector>
#include <string.h>
#include <poll.h>
#include "message.h"
#include "logging.c"
#include <map>
#include <iterator>
#include <sys/wait.h>

#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

using namespace std;

void server(vector<int> ends, map<int,int> endcid, int startBid, int minInc, int nOfBidders, map<int,int> endpid);

int main(int argc, char* argv[]){
    int startBid, minInc, nOfBidders;
    string path;
    int nOfArgs, clientCount = 1;

    string a;
    int b;

    vector<int> ends;
    map<int,int> endcid;
    map<int,int> endpid;
    

    int fd[2];
    
    cin >> startBid >> minInc >> nOfBidders;
   

    for (size_t i = 0; i < nOfBidders; i++)
    {
        cin >> path >> nOfArgs;
        vector<string> arr;

        for (size_t j = 0; j < nOfArgs; j++)
        {
            cin >> a;
            arr.push_back(a);
        }

        char * execArgs[1024];
        int argCount = 0;

        execArgs[argCount++] = (char*) path.c_str();
        for (size_t x = 0; x < arr.size(); x++)
        {
            execArgs[argCount++] = strdup(arr[x].c_str());
        }
        
        execArgs[argCount++] = (char*) NULL;

        const char * cstr = path.c_str();

        PIPE(fd);
        endcid.insert(pair<int,int> (fd[0],clientCount));
        ends.push_back(fd[0]);
        clientCount++;

        int pidFork = fork();


        if (pidFork == 0){
            dup2(fd[1],0);
            dup2(fd[1],1);
            close(fd[0]);
            int sss = execv(path.c_str(),execArgs);     
            return 0;
        }
        else {
            close(fd[1]);

            endpid.insert(pair<int,int> (fd[0],pidFork));
        }
        
    }
    
    server(ends, endcid, startBid, minInc, nOfBidders, endpid);

    return 0;
}


void server(vector<int> ends, map<int,int> endcid, int startBid, int minInc, int nOfBidders, map<int,int> endpid) {
    struct pollfd pfd[nOfBidders]; 
    
    cm* st = (cm* ) malloc(sizeof(cm));

    int r, n = nOfBidders, i;
    int currentBid = startBid;
    int minincrement = minInc;
    int finishedCount = 0;
    vector<int> closeOfBidders;
    vector<int> statusOfBidders;
    vector<int> sentStatus;
    
    int winnerCid = -1;

    for (size_t i = 0; i < nOfBidders; i++)
    {
        closeOfBidders.push_back(0);
        statusOfBidders.push_back(-1);
        sentStatus.push_back(-1);
    }
    

    for (size_t i = 0; i < nOfBidders; i++)
    {
        pfd[i] = {ends[i], POLLIN, 0};
    }

    while (finishedCount < nOfBidders) {
        poll(pfd, n, 0);

        for (size_t i = 0; i < n; i++)
        {
            if (closeOfBidders[i]){
                continue;
            }
            if (pfd[i].revents == 0 || POLLIN == 0){
                continue;
            }

            r = read(pfd[i].fd, st, sizeof(cm));

            if (r == 0) {
                pfd[i].fd = -1;
            }
            else {
                ii inpinfo;
                inpinfo.type = st->message_id;
                inpinfo.pid = endpid.find(ends[i])->second;
                inpinfo.info = st->params;

                oi outinfo;
                outinfo.pid = endpid.find(ends[i])->second;


                print_input(&inpinfo, endcid.find(ends[i])->second);

                if (st->message_id == 1){
                    sm respMsg;
                    smp parms;
                    cei info;
                    info.client_id = endcid.find(ends[i])->second;
                    info.current_bid = currentBid;
                    info.minimum_increment = minincrement;
                    info.starting_bid = startBid;
                    parms.start_info = info;
                    respMsg.message_id = SERVER_CONNECTION_ESTABLISHED;
                    respMsg.params = parms;

                    outinfo.type = SERVER_CONNECTION_ESTABLISHED;
                    outinfo.info = parms;
                    print_output(&outinfo, endcid.find(ends[i])->second);

                    write(ends[i],&respMsg,sizeof(sm));
                }
                else if (st->message_id == 2){
                    int tmpbid = st->params.bid;

                    sm respMsg;
                    smp parms;
                    bi info;
                    

                    if (tmpbid < startBid){
                        info.result = BID_LOWER_THAN_STARTING_BID;
                    }
                    else if (tmpbid < currentBid){
                        info.result = BID_LOWER_THAN_CURRENT;
                    }
                    else if ((tmpbid - currentBid) < minincrement){
                        info.result = BID_INCREMENT_LOWER_THAN_MINIMUM;
                    }
                    else {
                        currentBid = tmpbid;
                        info.result = BID_ACCEPTED;
                        winnerCid = endcid.find(ends[i])->second;
                    }
                    info.current_bid = currentBid;

                    parms.result_info = info;
                    respMsg.params = parms;
                    respMsg.message_id = SERVER_BID_RESULT;

                    outinfo.type = SERVER_BID_RESULT;
                    outinfo.info = parms;
                    print_output(&outinfo, endcid.find(ends[i])->second);

                    write(ends[i],&respMsg,sizeof(sm));
                }
                else if (st->message_id == 3){
                    finishedCount++;

                    sentStatus[i] = st->params.status;
                    closeOfBidders[i] = 1; //to indicate this bidder is closed.
                }
                else {
                    continue;
                }

            }
        }
        
    }
    

    print_server_finished(winnerCid, currentBid);

    sm respMsg;
    smp parms;
    wi info;
    info.winner_id = winnerCid;
    info.winning_bid = currentBid;
    parms.winner_info = info;
    respMsg.message_id = SERVER_AUCTION_FINISHED;
    respMsg.params = parms;

    oi outinfo;
    outinfo.type = SERVER_AUCTION_FINISHED;
    outinfo.info = parms;
    

    for (size_t i = 0; i < nOfBidders ; i++)
    {
        outinfo.pid = endpid.find(ends[i])->second;
        print_output(&outinfo, endcid.find(ends[i])->second);
        write(ends[i],&respMsg,sizeof(sm));
    }

    int tmpx;
    int tmpy;
    int tmpz;
    
    for (size_t i = 0; i < nOfBidders ; i++)
    {
        
        tmpx = ends[i];
        tmpy = endpid.find(tmpx)->second;
        tmpz = endcid.find(tmpx)->second;
        waitpid(tmpy, &statusOfBidders[i], 0);
        print_client_finished(tmpz, statusOfBidders[i], (statusOfBidders[i] == sentStatus[i]));
    }
    


}
