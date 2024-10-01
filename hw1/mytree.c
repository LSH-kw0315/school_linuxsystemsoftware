#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLEN 190
#define KILO 1024
#define FILEPERMISSION 10
#define MAXFILELEN 2000
#define STAIR 15

// I think, i have to use malloc.

void readDirectory(char *cwd, int *total, int spacedepth, int *line);
void getMode(mode_t mode, char *filePermission);
int compare(const void *a, const void *b);
int main(void) {
    /*
    1. open cwd
    2. read cwd
    3. read file and print file info
    3-1. if file that cpu read is directory, do 1,2 to that. and return to 3.
    */

    // open cwd

    char cwd[MAXLEN + 1];
    if (getcwd(cwd, MAXLEN) == NULL) {
        perror("failed to get the name of current working directory.");
        exit(-1);
    }
    // read file start.
    // for output.

    // index 0: total file, index 1: total directory
    int total[2] = {0, 0};

    int spacedepth = 0;
    int line[STAIR];
    memset((int *)line, 0, STAIR);

    readDirectory(cwd, total, spacedepth, line);

    // print total.

    printf("\n");
    switch (total[0]) {
    case 0:
    case 1:
        switch (total[1]) {
        case 0:
        case 1:
            printf("%d directory, %d file\n", total[1], total[0]);
            break;
        default:
            printf("%d directories, %d file.\n", total[1], total[0]);
            break;
        }
        break;
    default:
        switch (total[1]) {
        case 0:
        case 1:
            printf("%d directory, %d files\n", total[1], total[0]);
            break;
        default:
            printf("%d directories, %d files.\n", total[1], total[0]);
            break;
        }
        break;
    }
}
void readDirectory(char *cwd, int *total, int spacedepth, int *line) {
    // declare dirent struct and stat struct.

    DIR *dirp = (DIR *)malloc(sizeof(DIR *));
    dirp = opendir(cwd);

    struct stat *metadata = (struct stat *)malloc(sizeof(struct stat));

    struct passwd *userInfo;

    char curFileList[MAXFILELEN][MAXLEN];

    int curFileSize = 0;

    struct dirent *sizeCheck = (struct dirent *)malloc(sizeof(struct dirent));

    int *curline = (int *)malloc(sizeof(int) * STAIR);
    for (int i = 0; i < STAIR; i++) {
        curline[i] = line[i];
    }

    while ((sizeCheck = readdir(dirp)) != NULL) {

        memset((char *)curFileList[curFileSize], '\0', MAXLEN);
        strncpy(curFileList[curFileSize], sizeCheck->d_name,
                strlen(sizeCheck->d_name));

        curFileSize++;
    }
    curFileSize--;
    // printf("now i sort!\n");
    qsort((void *)curFileList, curFileSize + 1, sizeof(curFileList[0]),
          compare);
    free(sizeCheck);
    closedir(dirp);

    double fileSize;
    char sizePostfix = '\0';

    int offset = 0;
    while (offset <= curFileSize) {

        char *filepath = malloc((sizeof(char)) * (MAXLEN + 1));
        char *filePermission =
            (char *)malloc(sizeof(char) * (FILEPERMISSION + 1));
        // to check whether next entry is end of entry or not.

        // extract file name.

        memset((char *)filepath, '\0', MAXLEN + 1);
        strncpy(filepath, cwd, strlen(cwd));
        strcat(filepath, "/");
        strncat(filepath, curFileList[offset], strlen(curFileList[offset]));
        // printf("%s , %d\n", filepath, total[0] + 1);

        if (strncmp(curFileList[offset], ".", 1) == 0 ||
            strncmp(curFileList[offset], "..", 2) == 0) {
            free(filepath);

            offset++;
            continue;
        }

        DIR *detectErrorFile = (DIR *)malloc(sizeof(DIR *));
        detectErrorFile = opendir(cwd);
        struct dirent *errorFile =
            (struct dirent *)malloc(sizeof(struct dirent));

        int temp = 0;
        do {
            // printf("temp: %d offset: %d \n", temp, offset);

            errorFile = readdir(detectErrorFile);
            temp++;
        } while (temp <= offset);

        if (errorFile == NULL) {
            free(filepath);
            offset++;
            free(errorFile);
            closedir(detectErrorFile);
            continue;
        }
        closedir(detectErrorFile);

        for (int i = 0; i < spacedepth; i++) {
            if (curline[i] == 1) {
                printf("\u2502\t");
            } else {
                printf(" \t");
            }
        }

        // print information of file.

        if (lstat(filepath, metadata) < 0) {
            perror("failed to get file metadata.");
            exit(-1);
        }

        // get user information to present user name.

        userInfo = getpwuid(metadata->st_uid);

        // get file size. and adjust this value to tree program format.
        fileSize = metadata->st_size;

        if (fileSize > KILO * KILO * KILO) {
            sizePostfix = 'G';
            fileSize /= KILO * KILO;
        } else if (fileSize > KILO * KILO) {
            sizePostfix = 'M';
            fileSize /= KILO * KILO;

        } else if (fileSize > KILO) {
            sizePostfix = 'K';
            fileSize /= KILO;
        }
        //  to print user permission, convert the value of mode in stat
        //  structure.

        getMode(metadata->st_mode, filePermission);

        int dotCount = 0;
        for (int n = offset; n <= curFileSize; n++) {
            if (strncmp(curFileList[n], ".", 1) == 0 ||
                strncmp(curFileList[n], "..", 2) == 0) {
                dotCount++;
            }
        }

        if (offset == curFileSize || curFileSize - offset <= dotCount) {
            printf("\u2514\u2500\u2500 [ %ld %ld %s %s \t%2.1lf%c] %s\n",
                   metadata->st_ino, metadata->st_dev, filePermission,
                   userInfo->pw_name, round(fileSize), sizePostfix,
                   curFileList[offset]);
        } else {
            printf("\u251C\u2500\u2500 [ %ld %ld %s %s \t%2.1lf%c] %s\n",
                   metadata->st_ino, metadata->st_dev, filePermission,
                   userInfo->pw_name, round(fileSize), sizePostfix,
                   curFileList[offset]);
        }

        // if i read directory, read this directory too. (recursive)
        if (S_ISDIR(metadata->st_mode)) {
            DIR *errorOpeningDir = (DIR *)malloc(sizeof(DIR *));
            errorOpeningDir = opendir(filepath);
            if (offset == curFileSize || curFileSize - offset <= dotCount) {
                curline[spacedepth] = 0;
            } else {
                curline[spacedepth] = 1;
            }

            if (errorOpeningDir != NULL) {
                readDirectory(filepath, total, spacedepth + 1, curline);
            }
            closedir(errorOpeningDir);
            total[1]++;

        } else {
            total[0]++;
        }

        offset++;
        free(filepath);
        free(filePermission);
    }

    free(metadata);

    free(curline);
}
void getMode(mode_t mode, char *filePermission) {

    memset((char *)filePermission, '\0', FILEPERMISSION + 1);
    // dir part
    if (S_ISDIR(mode)) {
        strcat(filePermission, "d");
    } else if (S_ISLNK(mode)) {
        strcat(filePermission, "l");
    } else {
        strcat(filePermission, "-");
    }
    // user part
    if (S_IRUSR & mode) {
        strcat(filePermission, "r");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IWUSR & mode) {
        strcat(filePermission, "w");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IXUSR & mode) {
        strcat(filePermission, "x");
    } else {
        strcat(filePermission, "-");
    }

    // group part
    if (S_IRGRP & mode) {
        strcat(filePermission, "r");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IWGRP & mode) {
        strcat(filePermission, "w");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IXGRP & mode) {
        strcat(filePermission, "x");
    } else {
        strcat(filePermission, "-");
    }

    // other part
    if (S_IROTH & mode) {
        strcat(filePermission, "r");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IWOTH & mode) {
        strcat(filePermission, "w");
    } else {
        strcat(filePermission, "-");
    }

    if (S_IXOTH & mode) {
        strcat(filePermission, "x");
    } else {
        strcat(filePermission, "-");
    }
}
int compare(const void *a, const void *b) {

    return (strcmp((char *)a, (char *)b));
}
