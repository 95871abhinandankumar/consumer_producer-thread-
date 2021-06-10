#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define WRITER1 1500000
#define READER1 1500000
#define READER2 1500000
#define READER3 1500000
#define READER4 1500000
#define MAXCOUNT 5



//use of mutex -> to avoid race condition
//by using lock and unlock befor and after reading and writing

typedef struct {
	pthread_mutex_t *mut; 		 //mutex for avoiding race condition
	int writers;		  		//Number of Writers writing the data
	int readers;		 		  // number of Readersreading the data
	int waiting;		  		  // number of Writers waiting for their turn to write
	pthread_cond_t *writeOK;	  //when reader completed reading
	pthread_cond_t *readOK; 	   //When writer completed writing 
} rwl;


typedef struct {
	rwl* lock;					
	int id;
	long delay;   //time take to wait
} rwargs;

rwl* initlock (void);
rwargs* newRWargs (rwl *l, int i, long d);
void* reader (void *args);
void* writer (void *args);
void readlock (rwl *lock, int d);
void writelock (rwl *lock, int d);
void readunlock (rwl *lock);
void writeunlock (rwl *lock);
void deletelock (rwl *lock);

static int data = 1;

int main () {
	pthread_t r1, r2, r3, r4, w1;
	rwargs *a1, *a2, *a3, *a4, *a5;
	rwl *lock;
	
	lock = initlock ();  //Intilize the lock
	if(lock == NULL) exit(0);
	
	a1 = newRWargs (lock, 1, WRITER1);
	a2 = newRWargs (lock, 1, READER1);
	a3 = newRWargs (lock, 2, READER2);
	a4 = newRWargs (lock, 3, READER3);
	a5 = newRWargs (lock, 4, READER4);
	if(!(a1 && a2 && a3 && a4 && a5)) exit(0);

	//creating the Threads 
	pthread_create (&w1, NULL, &writer, a1);
	pthread_create (&r1, NULL, &reader, a2);
	pthread_create (&r2, NULL, &reader, a3);
	pthread_create (&r3, NULL, &reader, a4);
	pthread_create (&r4, NULL, &reader, a5);
   

    //wait for thread to complete
	pthread_join (w1, NULL);
	pthread_join (r1, NULL); 
	pthread_join (r2, NULL);
	pthread_join (r3, NULL);
	pthread_join (r4, NULL);
	
    //Always delete dyanamically alocated memory after using it

    
	free (a1);     //free memory allocated for a1
	free (a2);     //free memory allocated for a2
	free (a3);     //free memory allocated for a3
	free (a4);      //free memory allocated for a4
	free (a5);      //free memory allocated for a5
	deletelock(lock);
	return 0;
}
//end of main functiom


// Function initlock() to initlize reader and writer
rwl* initlock(void) {
	rwl *lock;
	//allocate the memory
	lock = (rwl*) malloc (sizeof (rwl));
	//If memory is not allocated retuen NULL
	if(lock == NULL) 
		return (NULL);

	lock->mut = (pthread_mutex_t*) malloc (sizeof (pthread_mutex_t));
	if(lock->mut == NULL) {
	 free(lock); 
	 return (NULL);
	  }

	lock->writeOK = (pthread_cond_t*) malloc (sizeof (pthread_cond_t));
	if(lock->writeOK == NULL) { 
		free (lock->mut); 
		free (lock); 
		return (NULL); 
	}

	lock->readOK = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
	if(lock->readOK == NULL) {
	 free (lock->mut); 
	 free (lock->writeOK); 
	 free (lock); return (NULL); 
	}

	pthread_mutex_init (lock->mut, NULL);
	pthread_cond_init (lock->writeOK, NULL);
	pthread_cond_init (lock->readOK, NULL);
	lock->readers = 0;
	lock->writers = 0;
	lock->waiting = 0;
	return (lock);
}//end of initlock()



//Initialize arguments for reader/writer function 
rwargs* newRWargs(rwl* l, int id, long d) {
	rwargs *args;

	args = (rwargs *)malloc (sizeof (rwargs));
	if (args == NULL) return (NULL);
	args->lock = l; args->id = id; args->delay = d;
	return (args);
}//end of newRWargs

// Read the data untill data=0 
void* reader(void* args) {
	rwargs *a;
	int d;

	a = (rwargs*)args;

	do {
		readlock (a->lock, a->id);
		d = data;
		//takes a->delay microseconds to read data
		usleep (a->delay);
		readunlock (a->lock);
		printf ("Reader %d : Data = %d\n", a->id, d);
		// Waits for a->delay microseconds before again writing data
		usleep (a->delay);
	} while (d != 0);
	printf ("Reader %d: Finished.\n", a->id);

	return (NULL);
}//end of reader

// writer function Writes into data
void* writer(void* args) {
	rwargs *a;
	int i;

	a = (rwargs*)args;

	for (i=2; i<MAXCOUNT; i++) {
		writelock (a->lock, a->id);
		data = i;
		// takes a->delay microseconds to write data 
		usleep (a->delay);
		writeunlock (a->lock);
		printf ("Writer %d: Wrote %d\n", a->id, i);
		//Waits for a->delay microseconds before again writing data 
		usleep (a->delay);
	}
	printf ("Writer %d: Finishing...\n", a->id);
	writelock (a->lock, a->id);
	data = 0;
	writeunlock (a->lock);
	printf ("Writer %d: Finished.\n", a->id);
	return (NULL);
}//end of writer


//starting of readlock function 
void readlock(rwl* lock, int d) {
	pthread_mutex_lock (lock->mut);
	// Wait until there is no one writing or waiting to write data 
	if (lock->writers || lock->waiting) {
		do {
			printf ("reader %d blocked.\n", d);
			pthread_cond_wait (lock->readOK, lock->mut);
			printf ("reader %d unblocked.\n", d);
		} while(lock->writers);
	}
	// Incraese the number of readers currently reading data 
	lock->readers++;
	pthread_mutex_unlock(lock->mut);
}//end of readlock


//starting of writelock
void writelock(rwl* lock, int d) {
	pthread_mutex_lock(lock->mut);
	//Increase the number of writers waiting to write data 
	lock->waiting++;
	//Wait until there is no one raeding data or writing data 
	while(lock->readers || lock->writers) {
		printf ("writer %d blocked.\n", d);
		pthread_cond_wait (lock->writeOK, lock->mut);
		printf ("writer %d unblocked.\n", d);
	}
	//Deccrease the number of writers waiting to write data 
	lock->waiting--;
	//Increase the number of writers writing data
	lock->writers++;
	pthread_mutex_unlock (lock->mut);
}//end of writelock


//readunlock function to unlock after completion
void readunlock(rwl* lock) {
	pthread_mutex_lock (lock->mut);
	//Decraese the number of readers currently reading data
	lock->readers--;
	//Signal that the writers can now write data
	pthread_cond_signal (lock->writeOK);
	pthread_mutex_unlock (lock->mut);
}//end of readunlock

void writeunlock(rwl* lock) {
	pthread_mutex_lock (lock->mut);
	// Decrease the number of writers writing data 
	lock->writers--;
	//Signals to all the readers that they can now read data 
	for(int i=0; i<4; i++)
	    pthread_cond_signal (lock->readOK);
	
	pthread_mutex_unlock (lock->mut);
}//end of writeunlock

//Destroy the mutex and condition objects and frees the allocated memory 
void deletelock (rwl *lock) {

	// destroy
	pthread_mutex_destroy (lock->mut);
	pthread_cond_destroy (lock->readOK);
	pthread_cond_destroy (lock->writeOK);

	//free all the allocated memory
	free(lock->mut); 
	free(lock->readOK); 
	free(lock->writeOK);
	free (lock);
}
