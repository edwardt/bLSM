
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include "logstore.h"
#include "datapage.cpp"
#include "logiterators.cpp"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#undef begin
#undef end

#include "check_util.h"

//template class DataPage<datatuple>;
template class treeIterator<datatuple>;

void insertProbeIter(int  NUM_ENTRIES)
{
    srand(1000);
    unlink("storefile.txt");
    unlink("logfile.txt");

    sync();

    bufferManagerNonBlockingSlowHandleType = IO_HANDLE_PFILE;

    Tinit();

    int xid = Tbegin();

    logtable ltable;

    int pcount = 5;
    ltable.set_fixed_page_count(pcount);
    recordid table_root = ltable.allocTable(xid);

    Tcommit(xid);
    
    xid = Tbegin();
    logtree *lt = ltable.get_tree_c1();
    
    recordid tree_root = lt->get_root_rec();


    std::vector<std::string> data_arr;
    std::vector<std::string> key_arr;
    preprandstr(NUM_ENTRIES, data_arr, 5*4096, true);
    preprandstr(NUM_ENTRIES+200, key_arr, 50, true);//well i can handle upto 200
    
    std::sort(key_arr.begin(), key_arr.end(), &mycmp);

    removeduplicates(key_arr);
    if(key_arr.size() > NUM_ENTRIES)
        key_arr.erase(key_arr.begin()+NUM_ENTRIES, key_arr.end());
    
    NUM_ENTRIES=key_arr.size();
    
    if(data_arr.size() > NUM_ENTRIES)
        data_arr.erase(data_arr.begin()+NUM_ENTRIES, data_arr.end());  
    
    
    
    
    printf("Stage 1: Writing %d keys\n", NUM_ENTRIES);

    
    int dpages = 0;
    int npages = 0;
    DataPage<datatuple> *dp=0;
    int64_t datasize = 0;
    std::vector<pageid_t> dsp;
    for(int i = 0; i < NUM_ENTRIES; i++)
    {
        //prepare the key
        datatuple newtuple;        
        uint32_t keylen = key_arr[i].length()+1;
        newtuple.keylen = &keylen;
        
        newtuple.key = (datatuple::key_t) malloc(keylen);
        for(int j=0; j<keylen-1; j++)
            newtuple.key[j] = key_arr[i][j];
        newtuple.key[keylen-1]='\0';

        //prepare the data
        uint32_t datalen = data_arr[i].length()+1;
        newtuple.datalen = &datalen;
        
        newtuple.data = (datatuple::data_t) malloc(datalen);
        for(int j=0; j<datalen-1; j++)
            newtuple.data[j] = data_arr[i][j];
        newtuple.data[datalen-1]='\0';

//        printf("key: \t, keylen: %u\ndata:  datalen: %u\n",
               //newtuple.key,
//               *newtuple.keylen,
               //newtuple.data,
//               *newtuple.datalen);

        datasize += newtuple.byte_length();

        if(dp == NULL)
        {
            dp = ltable.insertTuple(xid, newtuple, ltable.get_dpstate1(), lt);
            dpages++;
            dsp.push_back(dp->get_start_pid());
        }
        else
        {
            if(!dp->append(xid, newtuple))
            {
                npages += dp->get_page_count();
                delete dp;
                dp = ltable.insertTuple(xid, newtuple, ltable.get_dpstate1(), lt);
                dpages++;
                dsp.push_back(dp->get_start_pid());            
            }
        }

        free(newtuple.key);
        free(newtuple.data);
        
        
    }

    printf("\nTREE STRUCTURE\n");
    lt->print_tree(xid);
    
    printf("Total data set length: %d\n", datasize);
    printf("Storage utilization: %.2f\n", (datasize+.0) / (PAGE_SIZE * npages));
    printf("Number of datapages: %d\n", dpages);
    printf("Writes complete.\n");
    
    Tcommit(xid);
    xid = Tbegin();





    printf("Stage 2: Sequentially reading %d tuples\n", NUM_ENTRIES);

    
    int tuplenum = 0;
    treeIterator<datatuple> tree_itr(tree_root);


    datatuple *dt=0;
    while( (dt=tree_itr.getnext()) != NULL)
    {
        assert(*(dt->keylen) == key_arr[tuplenum].length()+1);
        assert(*(dt->datalen) == data_arr[tuplenum].length()+1);
        tuplenum++;
        free(dt->keylen);
        free(dt);
        dt = 0;
    }

    assert(tuplenum == key_arr.size());
    
    printf("Sequential Reads completed.\n");

    int rrsize=key_arr.size() / 3;
    printf("Stage 3: Randomly reading %d tuples by key\n", rrsize);

    for(int i=0; i<rrsize; i++)
    {
        //randomly pick a key
        int ri = rand()%key_arr.size();

        //get the key
        uint32_t keylen = key_arr[ri].length()+1;        
        datatuple::key_t rkey = (datatuple::key_t) malloc(keylen);
        for(int j=0; j<keylen-1; j++)
            rkey[j] = key_arr[ri][j];
        rkey[keylen-1]='\0';

        //find the key with the given tuple
        datatuple *dt = ltable.findTuple(xid, rkey, keylen, lt);

        assert(dt!=0);
        assert(*(dt->keylen) == key_arr[ri].length()+1);
        assert(*(dt->datalen) == data_arr[ri].length()+1);
        free(dt->keylen);
        free(dt);
        dt = 0;        
    }

    printf("Random Reads completed.\n");
    Tcommit(xid);
    Tdeinit();

}

/** @test
 */
int main()
{
    insertProbeIter(15000);

    
    
    return 0;
}
