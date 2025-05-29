#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>

sem_t* sem_trgovac;
sem_t* sem_kupci[3];
int* sastojci;  // 0 ili 1, je li sastojak na stolu
int poc = 1;


void pocetak() {
	printf("Kupac Sendviča 1: ima kruh\n");
	printf("Kupac Sendviča 2: ima šunku\n");
	printf("Kupac Sendviča 3: ima sir\n");
}

void trgovac() {
    srand(getpid());
    while (1) {
        sem_wait(sem_trgovac);

        // Resetiraj sve sastojke
        sastojci[0] = sastojci[1] = sastojci[2] = 0;

        int prvi = rand() % 3;
        int drugi;
        do {
            drugi = rand() % 3;
        } while (drugi == prvi);

        sastojci[prvi] = 1;
        sastojci[drugi] = 1;

        if ((prvi == 0 && drugi == 1) || (prvi == 1 && drugi == 0)) {
            printf("Trgovac stavlja: kruh i šunku\n");
        } else if ((prvi == 0 && drugi == 2) || (prvi == 2 && drugi == 0)) {
            printf("Trgovac stavlja: kruh i sir\n");
        } else {
            printf("Trgovac stavlja: šunku i sir\n");
        }

        for (int i = 0; i < 3; i++) {
            if (sastojci[i] == 0) {  // Nedostaje taj sastojak => taj kupac ima ono što fali
                sem_post(sem_kupci[i]);
                break;
            }
        }
    }
}

void kupac(int id) {
    while (1) {
        sem_wait(sem_kupci[id]);

		if (id == 0) {
			printf("Kupac sendviča 1: uzima sastojke, izlazi iz trgovine, slaže si sendvič i jede\n");
		} else if (id == 1) {
			printf("Kupac sendviča 2: uzima sastojke, izlazi iz trgovine, slaže si sendvič i jede\n");
		} else {
			printf("Kupac sendviča 3: uzima sastojke, izlazi iz trgovine, slaže si sendvič i jede\n");
		}

        usleep(100000);  // simulira pravljenje sendviča

        sem_post(sem_trgovac);
    }
}

int main() {
    // Očisti stare semafore
    sem_unlink("/sem_trgovac");
    sem_unlink("/sem_kupac_0");
    sem_unlink("/sem_kupac_1");
    sem_unlink("/sem_kupac_2");

	//ispis kupaca
	if (poc == 1) {
		pocetak();
		poc = 0;
	}

    // Dijeljena memorija za niz sastojaka
    sastojci = mmap(NULL, sizeof(int) * 3, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    // Semafori
    sem_trgovac = sem_open("/sem_trgovac", O_CREAT | O_EXCL, 0600, 1);
    if (sem_trgovac == SEM_FAILED) {
        perror("sem_open sem_trgovac");
        exit(1);
    }

    char sem_name[32];
    for (int i = 0; i < 3; i++) {
        snprintf(sem_name, sizeof(sem_name), "/sem_kupac_%d", i);
        sem_kupci[i] = sem_open(sem_name, O_CREAT | O_EXCL, 0600, 0);
        if (sem_kupci[i] == SEM_FAILED) {
            perror("sem_open sem_kupac");
            exit(1);
        }
    }

    pid_t djeca[4];

    // Trgovac
    if ((djeca[0] = fork()) == 0) {
        trgovac();
        exit(0);
    }

    // Kupci
    for (int i = 0; i < 3; i++) {
        if ((djeca[i + 1] = fork()) == 0) {
            kupac(i);
            exit(0);
        }
    }

    // Simulacija traje 5 sekundi
    sleep(5);

    // Prekini sve child procese
    for (int i = 0; i < 4; i++) {
        kill(djeca[i], SIGTERM);
    }

    // Pričekaj kraj procesa
    for (int i = 0; i < 4; i++) {
        waitpid(djeca[i], NULL, 0);
    }

    // Čisti semafore
    sem_close(sem_trgovac);
    sem_unlink("/sem_trgovac");
    for (int i = 0; i < 3; i++) {
        sem_close(sem_kupci[i]);
        snprintf(sem_name, sizeof(sem_name), "/sem_kupac_%d", i);
        sem_unlink(sem_name);
    }

    // Oslobodi memoriju
    munmap(sastojci, sizeof(int) * 3);

    return 0;
}
