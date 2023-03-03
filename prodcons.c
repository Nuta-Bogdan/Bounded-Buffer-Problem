

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "prodcons.h"

static ITEM buffer[BUFFER_SIZE];

static int expected_item = 0; //the item that should come up next
static int buffer_in = 0; //the position in the buffer where the item should be placed
static int buffer_out = 0; //the position in the buffer of the item that needs to be taken by the consumer

static void rsleep(int t);		 // already implemented (see below)
static ITEM get_next_item(void); // already implemented (see below)

//condition variables used for the synchronization
static pthread_cond_t cv_items[NROF_ITEMS] = {PTHREAD_COND_INITIALIZER};
static pthread_cond_t cv_produce = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cv_consume = PTHREAD_COND_INITIALIZER;

//mutexes used to ensure the items are put in and out in the right order in buffer
static pthread_mutex_t mutex_items[NROF_ITEMS] = {PTHREAD_MUTEX_INITIALIZER};
static pthread_mutex_t mutex_buffer = PTHREAD_MUTEX_INITIALIZER;

/* producer thread */
static void *
producer(void *arg)
{
	ITEM item;
	while (true /* TODO: not all items produced */)
	{
		// TODO:
		// * get the new item
		item = get_next_item();
		if (item == NROF_ITEMS)
		{
			break;
		}
		rsleep(100); // simulating all kind of activities...

		// if the next item is not the expected item we wait so that we put them in ascending order in the buffer
		if (item != expected_item)
		{
			pthread_mutex_lock(&mutex_items[item]);
			pthread_cond_wait(&cv_items[item], &mutex_items[item]);
			pthread_mutex_unlock(&mutex_items[item]);
		}
		// TODO:
		// * put the item into buffer[]
		//
		// follow this pseudocode (according to the ConditionSynchronization lecture):
		//      mutex-lock;
		//      while not condition-for-this-producer
		//          wait-cv;
		//      critical-section;
		//      possible-cv-signals;
		//      mutex-unlock;
		//
		// (see condition_test() in condition_basics.c how to use condition variables)

		pthread_mutex_lock(&mutex_buffer);
		while ((buffer_in + 1) % BUFFER_SIZE == buffer_out)
		{
			// the buffer is full
			pthread_cond_wait(&cv_produce, &mutex_buffer);
		}

		// producer puts the item in the buffer
		buffer[buffer_in] = item;
		buffer_in = (buffer_in + 1) % BUFFER_SIZE;
		expected_item++;

		if (expected_item < NROF_ITEMS)
		{
			pthread_mutex_lock(&mutex_items[expected_item]);
			pthread_cond_broadcast(&cv_items[expected_item]);
			pthread_mutex_unlock(&mutex_items[expected_item]);
		}

		pthread_cond_broadcast(&cv_consume);
		pthread_mutex_unlock(&mutex_buffer);
	}
	return (NULL);
}

/* consumer thread */
static void *
consumer(void *arg)
{
	ITEM item;
	while (true /* TODO: not all items retrieved from buffer[] */)
	{
		pthread_mutex_lock(&mutex_buffer);
		while (buffer_out == buffer_in)
		{
			// the buffer is empty
			pthread_cond_wait(&cv_consume, &mutex_buffer);
		}

		// consumer takes the item from the buffer
		item = buffer[buffer_out];
		buffer_out = (buffer_out + 1) % BUFFER_SIZE;

		pthread_cond_broadcast(&cv_produce);
		pthread_mutex_unlock(&mutex_buffer);

		// printing each item to stdout
		printf("%d\n", item);

		rsleep(100); // simulating all kind of activities...
		if (item == NROF_ITEMS - 1)
		{
			break;
		}
	}
	return (NULL);
}

int main(void)
{
	pthread_t producer_threads[NROF_PRODUCERS];//array of pointers to producer threads
	pthread_t consumer_thread;//pointer to consumer thread

	//creating the producer threads
	for (int i = 0; i < NROF_PRODUCERS; i++)
	{
		pthread_create(&producer_threads[i], NULL, producer, NULL);
	}

	//creating the consumer thread
	pthread_create(&consumer_thread, NULL, consumer, NULL);

	//waiting for the producer threads to finish
	for (int j = 0; j < NROF_PRODUCERS; j++)
	{
		pthread_join(producer_threads[j], NULL);
	}

	//waiting for the consumer thread to finish
	pthread_join(consumer_thread, NULL);

	return (0);
}


static void
rsleep(int t)
{
	static bool first_call = true;

	if (first_call == true)
	{
		srandom(time(NULL));
		first_call = false;
	}
	usleep(random() % t);
}


static ITEM
get_next_item(void)
{
	static pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
	static bool jobs[NROF_ITEMS + 1] = {false}; // keep track of issued jobs
	static int counter = 0;						// seq.nr. of job to be handled
	ITEM found;									// item to be returned

	pthread_mutex_lock(&job_mutex);

	counter++;
	if (counter > NROF_ITEMS)
	{
		// we're ready
		found = NROF_ITEMS;
	}
	else
	{
		if (counter < NROF_PRODUCERS)
		{
			// for the first n-1 items: any job can be given
			// e.g. "random() % NROF_ITEMS", but here we bias the lower items
			found = (random() % (2 * NROF_PRODUCERS)) % NROF_ITEMS;
		}
		else
		{
			// deadlock-avoidance: item 'counter - NROF_PRODUCERS' must be given now
			found = counter - NROF_PRODUCERS;
			if (jobs[found] == true)
			{
				// already handled, find a random one, with a bias for lower items
				found = (counter + (random() % NROF_PRODUCERS)) % NROF_ITEMS;
			}
		}

		// check if 'found' is really an unhandled item;
		// if not: find another one
		if (jobs[found] == true)
		{
			// already handled, do linear search for the oldest
			found = 0;
			while (jobs[found] == true)
			{
				found++;
			}
		}
	}
	jobs[found] = true;

	pthread_mutex_unlock(&job_mutex);
	return (found);
}
