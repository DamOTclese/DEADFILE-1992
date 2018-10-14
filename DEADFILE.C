
/* **********************************************************************
   * Deadfile.C - Fredric Rice, October 1994.                           *
   * The Skeptic Tank, 1:102/890.0  (818) 335-9601  24 Hours  9600bts   *
   *                                                                    *
   ********************************************************************** */
   
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <dos.h>
#include <dir.h>
#include <alloc.h>

/* **********************************************************************
   * Define macros                                                      *
   *                                                                    *
   ********************************************************************** */

#define skipspace(s)            while (isspace(*s))     ++s
#define The_Version             "2.00"
#define MAXIMUM_MESSAGES        1000
#define USHORT                  unsigned short
#define TRUE                    1
#define FALSE                   0

/* **********************************************************************
   * The message file format offered here is Fido format which has      *
   * been tested with OPUS and Dutchie. It represents the latest        *
   * format that I know about.                                          *
   *                                                                    *
   ********************************************************************** */

   struct fido_msg {
      char from[36];		/* Who the message is from		  */
      char to[36];		/* Who the message to to		  */
      char subject[72];		/* The subject of the message.		  */
      char date[20];            /* Message creation date/time             */
      USHORT times;             /* Number of time the message was read    */
      USHORT destination_node;  /* Intended destination node              */
      USHORT originate_node;    /* The originator node of the message     */
      USHORT cost;              /* Cost to send this message              */
      USHORT originate_net;     /* The originator net of the message      */
      USHORT destination_net;   /* Intended destination net number        */
      USHORT destination_zone;  /* Intended zone for the message          */
      USHORT originate_zone;    /* The zone of the originating system     */
      USHORT destination_point; /* Is there a point to destination?       */
      USHORT originate_point;   /* The point that originated the message  */
      USHORT reply;             /* Thread to previous reply               */
      USHORT attribute;         /* Message type                           */
      USHORT upwards_reply;     /* Thread to next message reply           */
   } message;			/* Something to point to this structure	  */

/* **********************************************************************
   * Define the errorlevel values we'll exit with.                      *
   *                                                                    *
   ********************************************************************** */

#define No_Problem              0
#define Cant_Find_Config_File   10
#define Configuration_Bad       11
#define Cant_Create_Log_File    12
#define Out_Of_Memory           13

/* **********************************************************************
   * Our data structure used to store message and file attach           *
   * information.                                                       *
   *                                                                    *
   ********************************************************************** */

    static struct Message_Information {
        char *file_name;                /* Name of the outbound file      */
        char keeper;                    /* Flag on whether it's a keeper  */
        char *message_subject;          /* The message's subject          */
    } message_info[MAXIMUM_MESSAGES];   /* Make a bunch of them.          */

/* **********************************************************************
   * Local data storage is next.                                        *
   *                                                                    *
   ********************************************************************** */

    static FILE *df_log;
    static char ever_send_date;
    static char the_date[26];
    static char total_files, total_messages;
    static USHORT want_diag;
    static USHORT want_log;

/* **********************************************************************
   * Configured and environment variable stuff gets stored here.        *
   *                                                                    *
   ********************************************************************** */

    static char configuration_directory[101];
    static char log_directory[101];
    static char mail_directory[101];
    static char outbound_directory[101];

/* **********************************************************************
   * Set the string offered to upper case                               *
   *                                                                    *
   ********************************************************************** */

static void ucase(char *this_record)
{
    short i;

    while (*this_record) {
        if (*this_record > 0x60 && *this_record < 0x7B) {
            *this_record = *this_record - 32;
        }

        this_record++;
    }
}

/* **********************************************************************
   * Make sure we only look at mail files.                              *
   *                                                                    *
   ********************************************************************** */

static char mail_file(char *this_name)
{
    while (*this_name && *this_name != '.')
        this_name++;

    if (! *this_name) return(0);
    if (! strncmp(this_name, ".MO", 3)) return(1);
    if (! strncmp(this_name, ".TU", 3)) return(1);
    if (! strncmp(this_name, ".WE", 3)) return(1);
    if (! strncmp(this_name, ".TH", 3)) return(1);
    if (! strncmp(this_name, ".FR", 3)) return(1);
    if (! strncmp(this_name, ".SA", 3)) return(1);
    if (! strncmp(this_name, ".SU", 3)) return(1);

    return(0);
}

/* **********************************************************************
   * See what's waiting in the outbound.                                *
   *                                                                    *
   ********************************************************************** */

static void scan_outbound(void)
{
    int result;
    struct ffblk direct;
    char outbound_search[101];

/*
 * Build an outbound directory to search through
 */

    (void)strcpy(outbound_search, outbound_directory);
    (void)strcat(outbound_search, "*.*");

/*
 * See if there is at least one
 */

    result = findfirst(outbound_search, &direct, FA_RDONLY | FA_ARCH);

    if (result != 0) {
        (void)printf("There are no files in: %s\n", outbound_directory);
        return;
    }

/*
 * Store it into the data structure
 */

    ucase(direct.ff_name);

    if (mail_file(direct.ff_name)) {
        message_info[total_files].file_name = (char *)farmalloc(14);

        if (message_info[total_files].file_name == (char *)NULL) {
            (void)printf("I ran out of memory!\n");
            fcloseall();
            exit(Out_Of_Memory);
        }

        (void)strcpy(message_info[total_files].file_name, direct.ff_name);

        if (want_diag) {
            (void)printf("DIAG: file %d: [%s]\n",
                total_files,
                message_info[total_files].file_name);
        }

        total_files++;
    }

/*
 * Go through the entire directory
 */

    while (result == 0) {
        result = findnext(&direct);

        if (result == 0) {
            ucase(direct.ff_name);

            if (mail_file(direct.ff_name)) {
                message_info[total_files].file_name = (char *)farmalloc(14);

                if (message_info[total_files].file_name == (char *)NULL) {
                    (void)printf("I ran out of memory!\n");
                    fcloseall();
                    exit(Out_Of_Memory);
                }

                (void)strcpy(message_info[total_files].file_name,
                    direct.ff_name);

                if (want_diag) {
                    (void)printf("DIAG: file %d: [%s]\n",
                        total_files,
                        message_info[total_files].file_name);
                }

                total_files++;
            }
        }
    }
}

/* **********************************************************************
   * Store the subject.                                                 *
   *                                                                    *
   ********************************************************************** */

static void plug_this_subject(char *file_name)
{
    FILE *msg_file;
    char path[101];

/*
 * Build a path and file name
 */

    (void)strcpy(path, mail_directory);
    (void)strcat(path, file_name);

/*
 * See if the file can be opened
 */

    if ((msg_file = fopen(path, "rb")) == (FILE *)NULL) {

        if (want_diag) {
            (void)printf("DIAG: file [%s] couldn't be opened\n", path);
        }

        return;
    }

/*
 * Read the header information
 */

    if (fread(&message, sizeof(struct fido_msg), 1, msg_file) != 1) {
        (void)fclose(msg_file);

        if (want_diag) {
            (void)printf("DIAG: file [%s] couldn't be read\n", path);
        }

        return;
    }

/*
 * Be sure to close the file
 */

    (void)fclose(msg_file);

/*
 * Store information about the file away
 */

    message_info[total_messages].message_subject =
       (char *)farmalloc(strlen(message.subject) + 2);

    if (message_info[total_messages].message_subject == (char *)NULL) {
        (void)printf("I ran out of memory!\n");
        fcloseall();
        exit(Out_Of_Memory);
    }

    ucase(message.subject);

    (void)strcpy(message_info[total_messages].message_subject,
        message.subject);

    if (want_diag) {
        (void)printf("DIAG: Message %d: [%s] [%s]\n",
            total_messages,
            path,
            message.subject);
    }

    total_messages++;
}

/* **********************************************************************
   * Scan mail directories.                                             *
   *                                                                    *
   ********************************************************************** */

static void scan_mail(void)
{
    USHORT result;
    struct ffblk direct;
    char mail_search[101];

/*
 * If there are no files, don't do this
 */

    if (total_files == 0) return;

/*
 * Build a search string
 */

    (void)strcpy(mail_search, mail_directory);
    (void)strcat(mail_search, "*.msg");

/*
 * See if there is at least one
 */

    result = findfirst(mail_search, &direct, FA_RDONLY | FA_ARCH);

    if (result != 0) {
        (void)printf("There are no messages in mail directory\n");
        return;
    }

/*
 * Store the information away
 */

    plug_this_subject(direct.ff_name);

/*
 * Continue to look for more
 */

    while (result ==0) {
        result = findnext(&direct);

        if (result == 0) {
            plug_this_subject(direct.ff_name);
        }
    }
}

/* **********************************************************************
   * Delete that which has been sent.                                   *
   *                                                                    *
   ********************************************************************** */

static void delete_sent(void)
{
    short count, loop;
    char file_path[101];
    char report[81];

/*
 * If there are no files, don't do this
 */

    if (total_files == 0)
        return;

/*
 * Go through the files and determine which should be deleted
 */

    for (count = 0; count < total_files; count++) {
        (void)strcpy(file_path, outbound_directory);
        (void)strcat(file_path, message_info[count].file_name);

        for (loop = 0;
            loop < total_messages && message_info[count].keeper == 0;
                loop++) {

            if (! strcmp(file_path, message_info[loop].message_subject)) {
                message_info[count].keeper = 1;

                (void)printf("Keeping: %s  %s\n",
                    file_path,
                    message_info[loop].message_subject);
            }
        }

        if (message_info[count].keeper == 0) {
            if (ever_send_date == 0) {
                if (want_log) {
                    (void)fputs(the_date, df_log);
                }

                ever_send_date = 1;
            }

            if (want_log) {
                (void)sprintf(report, "   File erase: %s\n", file_path);
                (void)fputs(report, df_log);
            }

            (void)printf("File %s erased\n", file_path);
            unlink(file_path);
        }
    }
}

/* **********************************************************************
   * Say hello to the world.                                            *
   *                                                                    *
   ********************************************************************** */

static void say_hello(void)
{
    (void)printf("\n%s Version %s (%s) %s",
        __FILE__, The_Version, __DATE__, the_date);

    (void)printf("    Mail:     %s\n", mail_directory);
    (void)printf("    Outbound: %s\n\n", outbound_directory);
}

/* **********************************************************************
   * Initialize this program.                                           *
   *                                                                    *
   ********************************************************************** */

static void initialize(void)
{
    USHORT count;
    long itstime;
    char *env;

/*
 * Get our environment variable to determine the path
 * to our configuration file and log file
 */

    if (NULL == (env = getenv("DEADFILE"))) {
        (void)strcpy(configuration_directory, "DEADFILE.CFG");
        (void)strcpy(log_directory, "DEADFILE.LOG");
    }
    else {
        (void)strcpy(configuration_directory, env);
        (void)strcpy(log_directory, env);

        if (configuration_directory[strlen(configuration_directory) - 1] != '\\') {
            (void)strcat(configuration_directory, "\\");
            (void)strcat(log_directory, "\\");
        }

        (void)strcat(configuration_directory, "DEADFILE.CFG");
        (void)strcat(log_directory, "DEADFILE.LOG");
    }

/*
 * Zero various things
 */

    want_diag = FALSE;
    want_log = FALSE;
    ever_send_date = total_files = total_messages = 0;
    mail_directory[0] = (char)NULL;
    outbound_directory[0] = (char)NULL;

    for (count = 0; count < MAXIMUM_MESSAGES; count++) {
        message_info[count].file_name = (char *)NULL;
        message_info[count].keeper = 0;
        message_info[count].message_subject = (char *)NULL;
    }

/*
 * Get the date and time
 */

    (void)time(&itstime);
    (void)strncpy(the_date, ctime(&itstime), 26);
}

/* **********************************************************************
   * Plug the mail directory.                                           *
   *                                                                    *
   ********************************************************************** */

static void plug_mail_directory(char *atpoint)
{
    (void)strcpy(mail_directory, atpoint);

    if (mail_directory[strlen(mail_directory) - 1] != '\\')
        (void)strcat(mail_directory, "\\");
}

/* **********************************************************************
   * Plug the outbound directory.                                       *
   *                                                                    *
   ********************************************************************** */

static void plug_outbound_directory(char *atpoint)
{
    (void)strcpy(outbound_directory, atpoint);

    if (outbound_directory[strlen(outbound_directory) - 1] != '\\')
        (void)strcat(outbound_directory, "\\");
}

/* **********************************************************************
   * Extract the configuration file information.                        *
   *                                                                    *
   ********************************************************************** */
   
static void extract_configuration(void)
{
    FILE *fin;
    char record[101], *atpoint;

/*
 * Open the configuration file
 */

    if ((fin = fopen(configuration_directory, "rt")) == (FILE *)NULL) {
        (void)printf("I can't find file: %s!\n", configuration_directory);
        fcloseall();
        exit(Cant_Find_Config_File);
    }

/*
 * Go through the file and extract configuration
 */

    while (! feof(fin)) {
        (void)fgets(record, 100, fin);

        if (! feof(fin)) {
            atpoint = record;
            skipspace(atpoint);
            atpoint[strlen(atpoint) - 1] = (char)NULL;

            if (! strnicmp(atpoint, "mail", 4)) {
                atpoint += 4;
                skipspace(atpoint);
                plug_mail_directory(atpoint);
            }
            else if (! strnicmp(atpoint, "outbound", 8)) {
                atpoint += 8;
                skipspace(atpoint);
                plug_outbound_directory(atpoint);
            }
            else if (! strnicmp(atpoint, "log", 3)) {
                atpoint += 3;
                skipspace(atpoint);

                if (! strnicmp(atpoint, "yes", 3)) {
                    want_log = TRUE;
                }
            }
        }
    }

/*
 * Make sure that the file is closed
 */

    (void)fclose(fin);

/*
 * See if something was missing
 */

    if (mail_directory[0] == (char)NULL) {
        (void)printf("No mail directory was defined in the configuration!\n");
        fcloseall();
        exit(Configuration_Bad);
    }

    if (outbound_directory[0] == (char)NULL) {
        (void)printf("No outbound directory was defined in the configuration!\n");
        fcloseall();
        exit(Configuration_Bad);
    }
}                                      

/* **********************************************************************
   * The main entry point.                                              *
   *                                                                    *
   ********************************************************************** */

void main(USHORT argc, char *argv[])
{
    USHORT count;

/*
 * Initialize ourself
 */

    initialize();

/*
 * See if we have command-line switches
 */

    for (count = 1; count < argc; count++) {
        if (! strnicmp(argv[count], "/diag", 5)) {
            want_diag = TRUE;
        }
    }

/*
 * Extract configuration
 */

    extract_configuration();

/*
 * Say hello
 */

    say_hello();

/*
 * Open the log file if it's wanted.
 */

    if (want_log) {
        if ((df_log = fopen(log_directory, "a+t")) == (FILE *)NULL) {
            (void)printf("Can't create log file: %s\n", log_directory);
            fcloseall();
            exit(Cant_Create_Log_File);
        }
    }

/*
 * Scan the outbound directory
 */

    scan_outbound();

/*
 * Scan the mail directory
 */

    scan_mail();

/*
 * Delete any outbound files which have no mail file
 */

    delete_sent();

/*
 * Close-up files.
 */

    if (want_log) {
        (void)fclose(df_log);
    }

/*
 * If there was no problem, say so!
 */

    exit(No_Problem);
}

