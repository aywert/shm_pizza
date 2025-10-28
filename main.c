#include "io.h"
#include <sys/shm.h>
#include <wait.h>
#include <sys/sem.h>

void Deliver(int id, int shmid, int semid, int N);
void CreateDelivery(int shmid, int semid, int N);
void Kitchener(int id, int shmid, int semid, int N);
void CreateKitchener(int shmid, int semid, int N);
void royal_print(int N, int* buffer);

int create_semaphore(const char* name, int flags, struct Argument* st);
void delete_semaphore(int semid);

struct sembuf wait_kitcheners[] = {{2, 0, 0}, {0, -1, 0}};     // waiting for entering

int main(int argc, char* argv[]) {

  struct Argument st;
  get_arg(argc, argv, &st);

  int shmid = shmget(ftok("main.c", 1), st.N*sizeof(int), IPC_CREAT | 0644);
  if (shmid < 0) {
    perror("shmget");
    exit(EXIT_FAILURE);
  }

  int semid = create_semaphore("main.c", IPC_CREAT, &st);

  int *buffer = shmat(shmid, NULL, 0);
  if (buffer == (int *)-1) {
    perror("shmat");
    exit(1);
  }

  for (int i = 0; i < st.N; i++)
    buffer[i] = free_table;

  royal_print(st.N, buffer);

  CreateKitchener(shmid, semid, st.K);
  CreateDelivery(shmid, semid, st.D);

  sleep(2);
  RunOp_safe(semid, wait_kitcheners, 2);

  printf("i am here\n");
  for (int i = 0; i < st.K+st.D; i++)
    wait(NULL);

  del_shm(shmid);
  delete_semaphore(semid);

  return 0;

}


void CreateKitchener(int shmid, int semid, int N) {
  for (int i = 0; i < N; i++) {
    pid_t pid = fork_safe();

    if (pid == -1) {
      fprintf(stderr, "fork: %s\n", strerror(errno));
      exit(1);
    }

    if (pid == 0) {
      Kitchener(i+1, shmid, semid, N);
      return;
    }
  } 
}

void CreateDelivery(int shmid, int semid, int N) {
  for (int i = 0; i < N; i++) {
    pid_t pid = fork_safe();

    if (pid == -1) {
      fprintf(stderr, "fork: %s\n", strerror(errno));
      exit(1);
    }

    if (pid == 0) {
      Deliver(i+1, shmid, semid, N);
      return;
    }
  } 
}

void Deliver(int id, int shmid, int semid, int N)
{
  struct sembuf delivery_enter[] = {{2, -1, 0}};// decreasing sem with numbers of kitcheners
  struct sembuf wait_enter[]     = {{0, 0, 0}};     // waiting for entering
  
  int* buffer = (int*)shmat(shmid, NULL, 0);

  if (buffer == (int*)-1) {
    perror("shmat");
    exit(EXIT_FAILURE);
  }
  
  printf("Deliver %d: Buffer address: %p\n", id, buffer);
  RunOp_safe(semid, delivery_enter, 1);
  RunOp_safe(semid, wait_enter, 1); //Z()&V()
  printf("Deliver %d: Ready!\n", id);

  exit(0);
}

void Kitchener(int id, int shmid, int semid, int N)
{
  struct sembuf kitchener_enter[] = {{1, -1, 0}};
  struct sembuf wait_enter[] = {{0, 0, 0}};// waiting for entering

  int* buffer = (int*)shmat(shmid, NULL, 0);

  if (buffer == (int*)-1) {
    perror("shmat");
    exit(EXIT_FAILURE);
  }
  
  printf("Cooker %d: Buffer address: %p\n", id, buffer);
 
  RunOp_safe(semid, kitchener_enter, 1);
  RunOp_safe(semid, wait_enter, 1);  //Z()&V()
  printf("Cooker %d: Ready!\n", id);

  exit(0);
}

int create_semaphore(const char* name, int flags, struct Argument* st) {
  // sem_array = | Able_to_enter | K | D |
  int semid = semget(ftok(name, 1), 3, flags | 0644);
  if (semid == -1) {
    perror("unable to create semphore\n");
    exit(EXIT_FAILURE);
  }

  semctl(semid, 0, SETVAL, 1);
  semctl(semid, 1, SETVAL, st->K);
  semctl(semid, 2, SETVAL, st->D);
  
  printf("Semaphore is created\n");

  return semid;
}

void delete_semaphore(int semid) {
  if (semctl(semid, IPC_RMID, 0) < 0) {
    perror("unable to delete semaphore\n");
    exit(EXIT_FAILURE);
  }

  printf("Semaphore is deleted\n");
}


void royal_print(int N, int* buffer) {
  for (int i = 0; i < N; i++) {
    printf("|");
    switch(buffer[i])
    {
      case pizza_ready:   printf("#");  break;
      case pizza_cooking: printf("...");break;
      case free_table:    printf("free");break;
      default: printf("|");break;
    }
  }

  printf("|\n");
}

int GetVal_safe(int semid, int num_in_sem) {
  int return_value = semctl(semid, num_in_sem, GETVAL);
  if (semctl(semid, num_in_sem, GETVAL) < 0) {
    perror("semop GetVal error\n");
    exit(EXIT_FAILURE);
  }

  return return_value;
}