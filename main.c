#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MIN_FILE_COUNT 2
#define MAX_FILE_COUNT 10

// Checks if the file exists in current directory
int file_exists(char* filename) {
    if(access(filename, F_OK) != -1) {
        return 1;
    } else {
        return 0;
    }
    return 0;
}

// Prompts the user the number of files he/she will input
int get_file_count() {
    int num_files;
    while(1) {
        printf("Number of files: ");
        scanf(" %d", &num_files);
        getchar();
        if(num_files >= MIN_FILE_COUNT && num_files <= MAX_FILE_COUNT) {
            break;
        } else {
            printf("\nInvalid number of files\n\n");
        }
    }
    return num_files;
}

// Prompts the user for k file names
char** get_filenames(char** files, int num_files) {
    int k;
    for(k = 0; k < num_files; k++) {
        while(1) {
            printf("Enter the name for file %d: ", k+1);
            char buffer[50];
            fgets(buffer, sizeof(buffer), stdin);
            strtok(buffer, "\n");
            files[k] = (char *)malloc(strlen(buffer)+1);
            strcpy(files[k], buffer);
            if(file_exists(files[k])) {
                break;
            } else {
                printf("\nCannot find file\n\n");
            }
        }
    }
    return files;
}

// Allows for program to shutdown gracefully
// and kills all processes before exit
void sig_handler(int sig_num) {
    fprintf(stdout, "Shutting Down Gracefully...\n");
    exit(0);
}

// Spawn child processes for concurrent file reading
void spawn_children(int k,char** files) {
    pid_t pids[k];
    int i,j,m;
    int l = 0;
    int fd[2*k];
    int to_parent[2*k];
    char buffer[50];
    char file[50];
    int total_count[k];
    int status;
    char key_word[50];

    // Initialize file descriptors for k children
    for (i = 0; i < k; i++) {
        pipe(&fd[2*i]);
        pipe(&to_parent[2*i]);
    }

    // Spawn children and read the key_word
    for(i = 0; i < k; i++) {
        total_count[i] = 0;
           
        if((pids[i] = fork()) < 0) {
            perror("fork");
            abort();
        } else if(pids[i] == 0) {
            signal(SIGINT,sig_handler);
            fprintf(stdout,"Spawning child %d for file %d\n",getpid(), i+1);
            close(fd[2*i+1]);
            close(to_parent[2*i]);
            read(fd[2*i],buffer,sizeof(buffer));
            read(fd[2*i],file,sizeof(file));
            fprintf(stdout,"Child %d (%d) read filename %s\n",getpid(),i+1,file);


            if(!strcmp(buffer,"$$$")) {
                kill(pids[i], SIGINT);
                signal(SIGINT,sig_handler);
            }
         
            // Each process reads their respective file
            // and removes all puncuation.
            // Increments count if key_word is the same as count
            FILE* fp = fopen(file,"r");
            char buf[50];
            while(fscanf(fp,"%s",buf) != EOF) {
                for(l=0;buf[l] != '\0'; ++l) {
                    while (!((buf[l] >= 'a' && buf[l] <= 'z') || 
                    ((buf[l] >= 'A' && buf[l] <= 'Z') || buf[l] == '\0'))) {
                        for(m = l; buf[m] != '\0';++m) {
                            buf[m] = buf[m+1];
                        }
                        buf[m] = '\0';
                    }
                }

                // If word and keyword match, increment count
                if(!strcmp(buf,buffer)) {
                    total_count[i]++;
                }
            }

            // Child write the wordcount to parent after read 
            // & exit once complete
            write(to_parent[2*i+1],(void *)&total_count[i],sizeof(total_count[i]));
            fprintf(stdout,"Child %d (%d) sending count: %d times\n",getpid(),i+1,total_count[i]);
            close(to_parent[2*i+1]);
            close(fd[2*i]);
            exit(0);
        }
    }

    printf("\nEnter a key word ($$$ to QUIT): \n\n");
    scanf (" %s", key_word);
    
    while(k > 0) {
        if(signal(SIGINT, sig_handler) == SIG_ERR) {
            fprintf(stdout, "Shutting down...\n");
        } 

        // If the keyword is $$$, kill all child processes 
        // gracefull shutdown
        if(!strcmp(key_word,"$$$")) {
            kill(pids[k], SIGINT);
            signal(SIGINT,sig_handler);
        }

        // Parent process writes keyword and filename
        // TO children
        if(sizeof(total_count) / 4 == k) {
            for (j = 0; j < k; j++) {
                close(fd[2*j]);
                close(to_parent[2*j+1]);
                write(fd[2*j+1],key_word,sizeof(key_word));
                write(fd[2*j+1],files[j],sizeof(files[j]));
                fprintf(stdout,"Parent writes filename %s to child\n",files[j]);
                close(fd[2*j+1]);
            }
        }  
        
        // Parent reads word count FROM children
        if(sizeof(total_count) / 4 == k) {
            for(j = 0; j < k; j++) {
                read(to_parent[2*j],(void *)&total_count[j],sizeof(total_count[j]));
                fprintf(stdout,"Parent reads word count from child %d\n",total_count[j]);
                close(to_parent[2*j]);
            }
        }

        // Log the termination of processes
        wait(&status);
        printf("PID %ld terminated with a return status of %d\n", (long) pids[k], status);
        --k;
    }

    // Print the formatted output
    fprintf(stdout,"\n          =====================================\n");
    fprintf(stdout,"             Key Word Count For Text Files     \n");
    fprintf(stdout,"          -------------------------------------\n");
    fprintf(stdout,"                      Key Word - %s    \n",key_word);
    fprintf(stdout,"          -------------------------------------    \n");
    for(i = 0; i < sizeof(total_count) / sizeof(int); i++) {
        fprintf(stdout,"             %s -- %d\n",files[i],total_count[i]);       
    }
    fprintf(stdout,"          =====================================\n");
}

// Begin the program
int main() {
    int num_files = get_file_count();
    char** files = malloc(num_files * sizeof(char *));
    files = get_filenames(files, num_files);

    // Counts key_word matches for the user from files
    // Until user opts out
    while(1) {
        spawn_children(num_files,files);
    }

    // Free memory allocation and exit program
    free(files);
    return 0;
}