#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <regex.h>


#define ASH_LINE_SIZE 64
#define ASH_BUFF_SIZE 16
#define ASH_TOKEN_COUNT 32
#define ASH_STR_DELIM " "

#define start() printf("\E[H")
#define clearscr() printf("\E[2J")

/* Structs */
struct program
{
	char *title;
	int number_of_arguments;
	char **arguments;
	char *input, *output;
	int output_type; /* 1 - rewrite, 2 - append */
	int background; /* 0 - no, 1 - yes */
	int pipeline; /* 0 - no pipeline, 1 - pipeline */
	int in;
	int out;
};


struct history_node
{
    int from;
    int to;
    char *line;
    int err;
    int jobs_end;
    pid_t pid_end;
    int status; /* 0 - running, 1 - stopped, 2 - done */
};



/* GLOBALS */
struct termios old_attr;
char *cur_usr = NULL;
char *cur_host = NULL;
int status = 1;
char *line;
int status_flg = 0;
int eof_flg = 0;
int cnt_of_progs = 0;
int tmp_cnt_of_progs = 0;
int cnt_of_jobs = 0;
int cnt_of_history = 0;
struct program **programs = NULL;
/*struct jobe_node jobs[3000];*/
struct history_node *hist = NULL;
char **history = NULL;
int buff_present = 0;
int line_present = 0;
pid_t pid;
pid_t pgid;


char *cur_dir = NULL;
int cur_dir_flg = 0;
int error_flg = 0;
int from = 0;
pid_t pid_to = -1;
/*int to = 0;*/
int process_status = 0;

void ignoreSigInt(int sgn)
{
    /*if (sgn == SIGINT)
    {
        cur_host = getenv("HOSTNAME");
	    if (cur_host == NULL)
	    {
		    cur_host = "ash";
	    }

        cur_dir = getcwd(NULL, 0);
        cur_dir_flg = 1;
	    cur_usr = getenv("USER");

        printf("\n");


    }*/

    if (sgn == SIGTSTP)
    {
        if (pid_to != -1)
        {
            kill(pid_to, SIGTSTP);
        }
    }

    /*signal(SIGINT, ignoreSigInt);
    signal(SIGTTOU, ignoreSigInt);*/
    signal(SIGTSTP, ignoreSigInt);
	return;
}

void clearWindow()
{
	start();
	clearscr();
	return;
}

void freeProgram(struct program *prg)
{
    int i;

    if (prg->title != NULL)
    {
        free(prg->title);
    }
    if (prg->number_of_arguments != 0)
    {
        for (i = 0; i < prg->number_of_arguments; ++i)
        {
            free(prg->arguments[i]);
        }
        free(prg->arguments);
    }
    if (prg->input != NULL)
    {
        free(prg->input);
    }
    if (prg->output != NULL)
    {
        free(prg->output);
    }
    free(prg);
}


char *getPathVariable(char *path)
{
	char *buffer = NULL;
	int i = 0;
    
    /* PID */
	if (strcmp("PID", path) == 0)
	{
		pid_t pid;
		pid = getpid();
		buffer = (char*)malloc(sizeof(char) * 60); /* hardcode for int */
		sprintf(buffer, "%d", pid);
		return buffer;
	}
	
	/* SHELL */
	if (strcmp("SHELL", path) == 0)
	{
		buffer = (char*)malloc(sizeof(char) * (strlen(ash_argv[0]) + 1));
		strcpy(buffer, ash_argv[0]);
		return buffer;
	}

	/* UID */
	if (strcmp("UID", path) == 0)
	{
		unsigned uid;
		uid = getuid();
		buffer = (char*)malloc(sizeof(char) * 60); /* hardcode for int */
		sprintf(buffer, "%u", uid);
		return buffer;
	}

    /* ? */
	if (strcmp("?", path) == 0)
	{
        buffer = (char*)malloc(60 * sizeof(char));
        sprintf(buffer, "%d", process_status); /* hardcode for int */
		/* last process status in last foreground job */
		return buffer; 
	}

	/* number */	
	int intpath;
	intpath = atoi(path);	
	if (intpath < ash_argc && intpath > 0)
	{
        buffer = (char*)malloc(60 * sizeof(char));
        sprintf(buffer, "%s", ash_argv[intpath]); /* hardcode for int */
		return buffer;
	}
    
    /* # */
	if (strcmp("#", path) == 0)
	{
		buffer = (char*)malloc(sizeof(char) * 60); /* hardcode for int */
		sprintf(buffer, "%d", ash_argc - 1);
		return buffer; 
	}
    
    
    /* PATH but not previous */
    if (getenv(path) != NULL)
    {
        buffer = (char*)malloc((1 + strlen(getenv(path))) * sizeof(char));
        strcpy(buffer, getenv(path));
        return buffer;
    }


    /* if no correct PATH variable found */
    if (getenv(path) == NULL)
    {
	    buffer = (char*)malloc(sizeof(char) * (strlen(path) + 2));
	    buffer[0] = '$';
	    for (i = 0; i < strlen(path); ++i)
	    {
		    buffer[i + 1] = path[i];
	    }
	    buffer[strlen(path) + 1] = '\0';
	    return buffer;
    }

		
    return buffer;
}

/* reading line from shell and storing it in the buffer */
char *readLine()
{
	/* ignoring SIGINT */
	signal(SIGINT, ignoreSigInt);

	int buffersize = 0;
	int pos = 0;
    char *buffer;
	buffer = (char*)malloc(sizeof(char));
	int c;

	if (buffer == NULL)
	{
		fprintf(stderr, "ash: cannot allocate memory buffer\n");
		exit(EXIT_FAILURE);
	}
			
	while (1)
	{
		c = getchar();	
			
        if (c == EOF)
        {
            eof_flg = 1;
            free(buffer);
            return NULL;
        }

		if (c == '\\')
		{	
			c = getchar();

			switch (c) 
			{
				case '\\':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\\';
					++pos;
					break;
				case 'n':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\n';
					++pos;
					break;
				case 't':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\t';
					++pos;
					break;
				case 'r':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\r';
					++pos;
					break;
				case '$':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '$';
					++pos;
					break;
				case '!':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '!';
					++pos;
					break;
				case '"':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '"';
					++pos;
					break;
				case '\'':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;
					break;
				case '#':
					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '#';
					++pos;
					break;
				case '\n':
					printf("> ");
					/*fflush(stdout);*/
					continue;
					break;	
				case '|':
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '|';
					++pos;
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					break;
				case ';':
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = ';';
					++pos;
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					break;
				case '&':
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '&';
					++pos;
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					break;
				case '<':
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '<';
					++pos;
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					break;
				case '>':
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '>';
					++pos;
                    /*if (pos + 1 >= buffersize)
					{
						buffersize += ASH_BUFF_SIZE;
						buffer = (char*)realloc(buffer, buffersize * sizeof(char));
					}
					buffer[pos] = '\'';
					++pos;*/

					break;
				
			}
			if (c != '\n' && c != EOF)
			{
				continue;
			}
		}

		if (c == '\'')
		{
			if (pos + 1 >= buffersize)
			{
				buffersize += ASH_BUFF_SIZE;
				buffer = (char*)realloc(buffer, buffersize * sizeof(char));
			}
			buffer[pos] = '\'';
			++pos;

			while ((c = getchar()) != EOF && c != '\'')
			{
				if (pos + 1 >= buffersize)
				{
					buffersize += ASH_BUFF_SIZE;
					buffer = (char*)realloc(buffer, buffersize * sizeof(char));
				}
				buffer[pos] = c;
				++pos;
				if (c == '\n')
				{
					printf("> ");	
				}
			}
			/*if (c == '\'')
			{
				if (pos + 1 >= buffersize)
				{
					buffersize += ASH_BUFF_SIZE;
					buffer = (char*)realloc(buffer, buffersize * sizeof(char));
				}
				buffer[pos] = '\'';
				++pos;	
			}*/
			
		}		

		if (c == '"')
		{
			char c2;
			if (pos + 1 >= buffersize)
			{
				buffersize += ASH_BUFF_SIZE;
				buffer = (char*)realloc(buffer, buffersize * sizeof(char));
			}
			buffer[pos] = '"';
			++pos;

			while ((c = getchar()) != EOF && c != '"')	
			{
						
				if (c == '\\')
				{	
					c2 = getchar();

					if (c2 == '\\')
					{
						if (pos + 1 >= buffersize)
						{
							buffersize += ASH_BUFF_SIZE;
							buffer = (char*)realloc(buffer, buffersize * sizeof(char));
						}
						buffer[pos] = '\\';
						++pos;
						continue;
					}
					if (c2 == '$')
					{
						if (pos + 1 >= buffersize)
						{
							buffersize += ASH_BUFF_SIZE;
							buffer = (char*)realloc(buffer, buffersize * sizeof(char));
						}
						buffer[pos] = '$';
						++pos;
						continue;
					}
					if (c2 != EOF)
					{
						if (pos + 1 >= buffersize)
						{
							buffersize += ASH_BUFF_SIZE;
							buffer = (char*)realloc(buffer, buffersize * sizeof(char));
						}
						buffer[pos] = c;
						++pos;
						if (pos + 1 >= buffersize)
						{
							buffersize += ASH_BUFF_SIZE;
							buffer = (char*)realloc(buffer, buffersize * sizeof(char));
						}	
						buffer[pos] = c2;
						++pos;
						if (c2 == '\n')
						{
							printf("> ");
						}
						continue;
					}
				}
				if (c == '$')
				{
					char *path = NULL;
					char *tmp = NULL;
					int i = 0;		
		
					c = getchar();
					path = (char*)malloc(sizeof(char));

					while (c != ' '
						&& c != '\n'
						&& c != '\''
						&& c != '"'
						&& c != ';'
						&& c != '&'
						&& c != '<'
						&& c != '>'
						&& c != '!'
						&& c != '$'
						&& c != ':')
					{
						path[i] = c;
						++i;
						path = (char*)realloc(path, sizeof(char) * (i + 1));
						c = getchar();	
					}
					/*if (c == '"')
					{
						path[i] = '"';
						++i;
						path = (char*)realloc(path, sizeof(char) * (i + 1));
					}*/	
					path[i] = '\0';
			
					tmp = getPathVariable(path);
					free(path);
					for (i = 0; i < strlen(tmp); ++i)
					{
						if (pos + 1 >= buffersize)
						{
							buffersize += ASH_BUFF_SIZE;
							buffer = (char*)realloc(buffer, buffersize * sizeof(char));
						}
						buffer[pos] = tmp[i];
						++pos;
					}
				free(tmp);
				if (c == '"')
				{
					break;
				}
				/*if (c == '\n')
				{
					printf("> ");
				}
				continue;*/
				}			
				if (pos + 1 >= buffersize)
				{
					buffersize += ASH_BUFF_SIZE;
					buffer = (char*)realloc(buffer, buffersize * sizeof(char));
				}	
				buffer[pos] = c;
				++pos;	
				if (c == '\n')
				{
					printf("> ");
				}
			}			
		}

		if (c == '$')
		{
			char *path = NULL;
			char *tmp = NULL;
			int i = 0;		
		
			c = getchar();
			path = (char*)malloc(sizeof(char));

			while (c != ' '
					&& c != '\n'
					&& c != '\''
					&& c != '"'
					&& c != ';'
					&& c != '&'
					&& c != '<'
					&& c != '>'
					&& c != '!'
					&& c != '$'
					&& c != ':')
			{
				path[i] = c;
				++i;
				path = (char*)realloc(path, sizeof(char) * (i + 1));
				c = getchar();	
			}	
			path[i] = '\0';
			
			tmp = getPathVariable(path);
			for (i = 0; i < strlen(tmp); ++i)
			{
				if (pos + 1 >= buffersize)
				{
					buffersize += ASH_BUFF_SIZE;
					buffer = (char*)realloc(buffer, buffersize * sizeof(char));
				}
				buffer[pos] = tmp[i];
				++pos;
			}
			free(tmp);
			free(path);
		}	

        if (c == '!')
        {
            char *tmp = NULL;
            int i = 0;

            c = getchar();
            tmp = (char*)malloc(sizeof(char));

            while (c >= '0' && c <= '9')
            {
                tmp[i] = c;
                ++i;
                tmp = (char*)realloc(tmp, sizeof(char) * (i + 1));
                c = getchar();
            }
            tmp[i] = '\0';

            if (atoi(tmp) > 0 && atoi(tmp) <= cnt_of_history)
            {
                for (i = 0; i < strlen(hist[atoi(tmp) - 1].line); ++i)
			    {
				    if (pos + 1 >= buffersize)
				    {
					    buffersize += ASH_BUFF_SIZE;
					    buffer = (char*)realloc(buffer, buffersize * sizeof(char));
				    }
				    buffer[pos] = hist[atoi(tmp) - 1].line[i];
				    ++pos;
			    }

            }
            free(tmp);
        }

		if (c == '#')
		{
            while ((c = getchar()) != '\n' && c != EOF) { }
			if (pos + 1 >= buffersize)
			{
				buffersize += ASH_BUFF_SIZE;
				buffer = (char*)realloc(buffer, buffersize * sizeof(char));
			}
			buffer[pos] = '\0';
			++pos;
			return buffer;
		}		

		if (c == EOF || c == '\n')
		{
			if (pos + 1 >= buffersize)
			{
				buffersize += ASH_BUFF_SIZE;
				buffer = (char*)realloc(buffer, buffersize * sizeof(char));
			}
			buffer[pos] = '\0';
			++pos;
			return buffer;
		}

		if (pos + 1 >= buffersize)
		{
			buffersize += ASH_BUFF_SIZE;
			buffer = (char*)realloc(buffer, buffersize * sizeof(char));
		}
		buffer[pos] = c;
		++pos;
	}

    /*free(buffer);*/
    /*if (buffer != NULL)
    {
        free(buffer);
    }*/
}


    
void parseLine(/*char *line*/)
{
	signal(SIGINT, ignoreSigInt);
	
	if (line == NULL)
	{
		fprintf(stderr, "ash: line is NULL in parseLine\n");
		exit(EXIT_FAILURE);
	}	

    /*if (!strcmp(line, "exit"))
    {
        status = 0;
        free(line);
        return;
    }*/

	int pos = 0;
	int len = 0;
	/*int cnt_of_input = 0;
	int cnt_of_output = 0;*/
	int i = 0;
    int buffer_size = ASH_BUFF_SIZE;
    int in = 0, out = 0;
	char *buffer = NULL;
	struct program *tmp_prog = NULL;
    len = strlen(line);

    if (len == 0)
    {
        /*if (line != NULL)
        {
            free(line);
        }*/
        return;
    }


	/* beginning of creating program struct */
	tmp_prog = (struct program*)malloc(sizeof(struct program));

	if (tmp_prog == NULL)
	{
		fprintf(stderr, "ash: cannot allocate memory for tmp_prog\n");
		exit(EXIT_FAILURE);
	}
	
	/* creating buffer */
	if ((buffer = (char*)malloc(sizeof(char) * buffer_size)) == NULL)
	{
		fprintf(stderr, "ash: cannot allocate memory for buffer\n");
		exit(EXIT_FAILURE);
	}
    buffer[0] = '\0';
    buff_present = 1;
	
	/*printf("\nline: %s\n", line);*/

    /* adding ';' for the end of the line */
    if ((len >= 1) && (line[len - 1] != '&' && line[len - 1] != ';' && line[len - 1] != '|'))
    {
        ++len;
        line = (char*)realloc(line, sizeof(char) * (len + 1));
        line[len - 1] = ';';
        line[len] = '\0'; 
    }

    /*printf("after: %s\n", line);*/

	i = 0;
    /* input default preferences to tmp_prog */
	tmp_prog->title = NULL;		
	tmp_prog->number_of_arguments = 0;
	tmp_prog->input = NULL;
	tmp_prog->output = NULL;
	tmp_prog->arguments = NULL;
	tmp_prog->pipeline = 0;
	tmp_prog->output_type = 0;
	tmp_prog->background = 0;
	tmp_prog->in = 0;
	tmp_prog->out = 0;

	while (pos < len)
	{
				
		        
        switch (line[pos])
        {
            case '"':
                ++pos;
                while (line[pos] != '"' && line[pos] != '\0')
                {
                    if (line[pos] == '\\' && line[pos + 1] == '"')
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
                        ++pos;
		                buffer[i] = line[pos];
                        ++i;
		                ++pos;
                        continue;
                    }
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = line[pos];
                    ++i;
		            ++pos;
                }
                ++pos;
                /*--len;
                --len;*/
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;
                        if (pos < len)
                        {
                            buffer = (char*)malloc(sizeof(char));
                            buff_present = 1;
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buffer = (char*)malloc(sizeof(char));
                            buff_present = 1;

                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        buff_present = 1;

                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                        buff_present = 1;

                    }

                    out = 0;
                }

                break;
            case '\'':
                ++pos;
                while (line[pos] != '\'' && line[pos] != '\0')
                {
                    if (line[pos] == '\\' && line[pos + 1] == '\'')
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
                        ++pos;
		                buffer[i] = line[pos];
                        ++i;
		                ++pos;
                        continue;
                    }
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = line[pos];
                    ++i;
		            ++pos;
                }
                ++pos;
                /*--len;
                --len;*/
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            buff_present = 1;

                            i = 0;    
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buffer = (char*)malloc(sizeof(char));
                        buff_present = 1;

                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }

                break;
            case '\\':
                if (line[pos + 1] == ' ')
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
                    ++pos;
		            buffer[i] = line[pos];
                    ++i;
		            ++pos;
                    continue; 
                }
                else
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = line[pos];
                    ++i;
		            ++pos;
                }
                break;
            case ' ':
                ++pos;                   
                /* if we have title of the program */
                if (tmp_prog->title != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    ++tmp_prog->number_of_arguments;
                    /*if (tmp_prog->arguments == NULL)
                    {
                         allocating memory for tmp_prog->arguments 
	                    if ((tmp_prog->arguments = (char**)malloc(sizeof(char*) * buffer_size_for_args)) == NULL)
	                    {
		                    fprintf(stderr, "ash: cannot allocate memory for arguments\n");
		                    exit(EXIT_FAILURE);
	                    }
		
	                     NULL to all tmp_prog->arguments 
	                    int t = 0;
	                    while (t < buffer_size_for_args)
	                    {
		                    tmp_prog->arguments[t] = NULL;
		                    ++t;
	                    }

                    }*/
                    tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                    tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                    strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        buffer_size = 0;
                        i = 0;
                    }
                }
                else if (tmp_prog->title == NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    buffer[i] = '\0';
                    ++i;
                    tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                    strcpy(tmp_prog->title, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        buffer_size = 0;
                        i = 0;
                    }
                } 
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                break;
            case '>':
                ++pos;
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                if (line[pos] == '>')
                {
                    ++pos;
                    tmp_prog->output_type = 2; /* append */
                } 
                else
                {
                    tmp_prog->output_type = 1; /* rewrite */
                } 
                out = 1;
                tmp_prog->out = 1;
                break;
            case '<':
                ++pos;
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                in = 1;
                tmp_prog->in = 1;
                break;
            case '|':
                ++pos;
                tmp_prog->pipeline = 1;
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                if (/*buffer != NULL &&*/tmp_prog->output == NULL && in == 0 && out == 0 /*&& strlen(buffer) != 0*/)
                {
                    if (tmp_prog->output == NULL)
                    {
                        tmp_prog->output = (char*)malloc(sizeof(char) * 2);
                    }
                    else
                    {
                        /*free(tmp_prog->output);*/
                        tmp_prog->output = (char*)realloc(tmp_prog->output, sizeof(char) * 2);
                    }
                    tmp_prog->output[0] = 'n';
                    tmp_prog->output[1] = '\0';
                }

                if (tmp_prog->input == NULL && in == 0 && out == 0)
                {
                    tmp_prog->input = (char*)malloc(sizeof(char) * 6); /* stdin */
                    strcpy(tmp_prog->input, "stdin");
                    /*tmp_prog->input = "stdin";
                    tmp_prog->input[5] = '\0';*/
                }
                
                if (tmp_prog->title != NULL && tmp_prog->input != NULL && tmp_prog->output != NULL)
                {
                    tmp_prog->background = 0;
                    ++cnt_of_progs;
                    programs = (struct program**)realloc(programs, 
                        sizeof(struct program*) * cnt_of_progs);
                
                    programs[cnt_of_progs - 1] = tmp_prog;
                    ++tmp_cnt_of_progs;
                }
                else
                {
                    error_flg = 1;
                    freeProgram(tmp_prog);
                }
                
                /* new tmp_prog */
                if (pos < len)
                {
                    tmp_prog = (struct program*)malloc(sizeof(struct program));
                    tmp_prog->title = NULL;		
		            tmp_prog->number_of_arguments = 0;
		            tmp_prog->input = NULL;
		            tmp_prog->output = NULL;
		            tmp_prog->arguments = NULL;
		            tmp_prog->pipeline = 0;
		            tmp_prog->output_type = 0;
		            tmp_prog->background = 0;
		            tmp_prog->in = 0;
		            tmp_prog->out = 0;
                }
 
                break;
            case ';':
                ++pos;
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }


                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0) 
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                if (tmp_prog->input == NULL && in == 0 && out == 0)
                {
                    tmp_prog->input = (char*)malloc(sizeof(char) * 6); /* stdin */
                    strcpy(tmp_prog->input, "stdin");
                    /*tmp_prog->input = "stdin";
                    tmp_prog->input[5] = '\0';*/
                }
                if (tmp_prog->output == NULL && in == 0 && out == 0)
                {
                    tmp_prog->output = (char*)malloc(sizeof(char) * 7); /* stdout */
                    strcpy(tmp_prog->output, "stdout");
                    /*tmp_prog->output = "stdout";
                    tmp_prog->output[6] = '\0';*/
                }
                if (tmp_prog->title != NULL && tmp_prog->input != NULL && tmp_prog->output != NULL)
                {
                    ++cnt_of_progs;
                    programs = (struct program**)realloc(programs, 
                        sizeof(struct program*) * cnt_of_progs);
                
                    programs[cnt_of_progs - 1] = tmp_prog;
                    ++tmp_cnt_of_progs;
                }
                else 
                {
                    error_flg = 1;
                    freeProgram(tmp_prog);
                    /*if (tmp_prog->title != NULL)
                    {
                        free(tmp_prog->title);
                        tmp_prog->title = (char*)malloc(sizeof(char));
                        tmp_prog->title[0] = '\0';
                    }
                    if (tmp_prog->input != NULL)
                    {
                        free(tmp_prog->input);
                        tmp_prog->input = (char*)malloc(sizeof(char));
                        tmp_prog->input[0] = '\0';

                    }
                    if (tmp_prog->output != NULL)
                    {
                        free(tmp_prog->output);
                        tmp_prog->output = (char*)malloc(sizeof(char));
                        tmp_prog->output[0] = '\0';
                    }
                    if (tmp_prog->number_of_arguments != 0)
                    {
                        int i;
                        for (i = 0; i < tmp_prog->number_of_arguments; ++i)
                        {
                            free(tmp_prog->arguments[i]);
                        }
                        free(tmp_prog->arguments);
                        tmp_prog->arguments = (char**)malloc(sizeof(char*));
                    }
    
                    tmp_prog->number_of_arguments = 0;
                    tmp_prog->pipeline = 0;
		            tmp_prog->output_type = 0;
		            tmp_prog->background = 0;
		            tmp_prog->input_flg = 0;
		            tmp_prog->output_flg = 0;*/

                }


                /*++cnt_of_progs;
                programs = (struct program**)realloc(programs, 
                    sizeof(struct program*) * cnt_of_progs);
                
                programs[cnt_of_progs - 1] = tmp_prog;*/
                
                /* new tmp_prog */
                if (pos < len)
                {
                    tmp_prog = (struct program*)malloc(sizeof(struct program));
                    tmp_prog->title = NULL;		
		            tmp_prog->number_of_arguments = 0;
		            tmp_prog->input = NULL;
		            tmp_prog->output = NULL;
		            tmp_prog->arguments = NULL;
		            tmp_prog->pipeline = 0;
		            tmp_prog->output_type = 0;
		            tmp_prog->background = 0;
		            tmp_prog->in = 0;
		            tmp_prog->out = 0;
                }
 
                break;

            case '&':
                ++pos;
                if (buffer != NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    if (tmp_prog->title == NULL)
                    {
                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                        strcpy(tmp_prog->title, buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }
                    else
                    {
                        ++tmp_prog->number_of_arguments;
                        tmp_prog->arguments = (char**)realloc(tmp_prog->arguments, sizeof(char*) * (tmp_prog->number_of_arguments + 1));
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = NULL; 

                        if (i >= buffer_size)
		                {
			                buffer_size += ASH_BUFF_SIZE;
			                buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		                }
		                buffer[i] = '\0';
                        ++i;
                        tmp_prog->arguments[tmp_prog->number_of_arguments - 1] = (char*)realloc(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], i * sizeof(char));
                        strcpy(tmp_prog->arguments[tmp_prog->number_of_arguments - 1], buffer);
                        free(buffer);
                        buff_present = 0;

                        if (pos < len)
                        {
                            buff_present = 1;

                            buffer = (char*)malloc(sizeof(char));
                            buffer[0] = '\0';
                            i = 0;
                            buffer_size = 0;
                        }

                    }

                }
                if (buffer != NULL && in == 1 && out == 0 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    
                    tmp_prog->input = (char*)realloc(tmp_prog->input, i * sizeof(char));
                    strcpy(tmp_prog->input, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    in = 0;
                }
                if (buffer != NULL && in == 0 && out == 1 && strlen(buffer) != 0)
                {
                    if (i >= buffer_size)
		            {
			            buffer_size += ASH_BUFF_SIZE;
			            buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		            }
		            buffer[i] = '\0';
                    ++i;
                    tmp_prog->output = (char*)realloc(tmp_prog->output, i * sizeof(char));
                    strcpy(tmp_prog->output, buffer);
                    free(buffer);
                    buff_present = 0;

                    if (pos < len)
                    {
                        buff_present = 1;

                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;
                    }

                    out = 0;
                }
                if (tmp_prog->input == NULL && in == 0 && out == 0)
                {
                    tmp_prog->input = (char*)malloc(sizeof(char) * 6); /* stdin */
                    strcpy(tmp_prog->input, "stdin");
                    /*tmp_prog->input = "stdin";
                    tmp_prog->input[5] = '\0';*/
                }
                if (tmp_prog->output == NULL && in == 0 && out == 0)
                {
                    tmp_prog->output = (char*)malloc(sizeof(char) * 7); /* stdout */
                    strcpy(tmp_prog->output, "stdout");
                    /*tmp_prog->output = "stdout";
                    tmp_prog->output[6] = '\0';*/
                }
                
                if (tmp_prog->title != NULL && tmp_prog->input != NULL && tmp_prog->output != NULL)
                {
                    tmp_prog->background = 1;
                    ++cnt_of_progs;
                    programs = (struct program**)realloc(programs, 
                        sizeof(struct program*) * cnt_of_progs);
                
                    programs[cnt_of_progs - 1] = tmp_prog;
                    ++tmp_cnt_of_progs;
                }
                else 
                {
                    error_flg = 1;
                    freeProgram(tmp_prog);
                    /*if (tmp_prog->title != NULL)
                    {
                        free(tmp_prog->title);
                        tmp_prog->title = (char*)malloc(sizeof(char));
                        tmp_prog->title[0] = '\0';
                    }
                    if (tmp_prog->input != NULL)
                    {
                        free(tmp_prog->input);
                        tmp_prog->input = (char*)malloc(sizeof(char));
                        tmp_prog->input[0] = '\0';

                    }
                    if (tmp_prog->output != NULL)
                    {
                        free(tmp_prog->output);
                        tmp_prog->output = (char*)malloc(sizeof(char));
                        tmp_prog->output[0] = '\0';
                    }
                    if (tmp_prog->number_of_arguments != 0)
                    {
                        int i;
                        for (i = 0; i < tmp_prog->number_of_arguments; ++i)
                        {
                            free(tmp_prog->arguments[i]);
                        }
                        free(tmp_prog->arguments);
                        tmp_prog->arguments = (char**)malloc(sizeof(char*));
                    }
    
                    tmp_prog->number_of_arguments = 0;
                    tmp_prog->pipeline = 0;
		            tmp_prog->output_type = 0;
		            tmp_prog->background = 0;
		            tmp_prog->input_flg = 0;
		            tmp_prog->output_flg = 0;*/
                
                }
                
                /* new tmp_prog */
                if (pos < len)
                {
                    tmp_prog = (struct program*)malloc(sizeof(struct program));
                    tmp_prog->title = NULL;		
		            tmp_prog->number_of_arguments = 0;
		            tmp_prog->input = NULL;
		            tmp_prog->output = NULL;
		            tmp_prog->arguments = NULL;
		            tmp_prog->pipeline = 0;
		            tmp_prog->output_type = 0;
		            tmp_prog->background = 0;
		            tmp_prog->in = 0;
		            tmp_prog->out = 0;
                }
                break; 


            default:
                if (i >= buffer_size)
		        {
			        buffer_size += ASH_BUFF_SIZE;
			        buffer = (char*)realloc(buffer, buffer_size * sizeof(char));
		        }
		        buffer[i] = line[pos];
                ++i;
		        ++pos;
                break;

        }
	}
    if (buff_present)
    {
        free(buffer);
    }
    for (i = 0; i < cnt_of_progs; ++i)
    {/*
         crutch 
        if (programs[i]->number_of_arguments == 1)
        {
            if (strlen(programs[i]->arguments[0]) == 0)
            {
                --programs[i]->number_of_arguments;
                free(programs[i]->arguments[0]);
            }
        }
        */
        /*if (programs[i]->title != NULL && strlen(programs[i]->title) == 0)
        {
            if (programs[i]->arguments[0] != NULL)
            {
                programs[i]->title = (char*)realloc(programs[i]->title, (strlen(programs[i]->arguments[0]) + 1) * sizeof(char));
                strcpy(programs[i]->title, programs[i]->arguments[0]);
                free(programs[i]->arguments[0]);
                --programs[i]->number_of_arguments;
                int j = 0;
                for (j = 0; j < programs[i]->number_of_arguments; ++j)
                {
                    programs[i]->arguments[j] = programs[i]->arguments[j + 1];
                }
                if (j != 0)
                {
                    free(programs[i]->arguments[j]);
                }
            }
        }
        if (programs[i]->title == NULL)
        {
            int j = 0;
            --cnt_of_progs;
            for (j = 0; j < programs[i]->number_of_arguments; ++j)
            {
                programs[i]->arguments[j] = programs[i]->arguments[j + 1];
            }
            if (j != 0)
            {
                free(programs[i]->arguments[j]);
            }
            if (programs[i]->input != NULL)
            {
                free(programs[i]->input);
            }
            if (programs[i]->output != NULL)
            {
                free(programs[i]->output);
            }
        }*/
        if (programs[i]->out == 1 && programs[i]->pipeline == 1)
        {
            if ((i + 1 < cnt_of_progs) && (programs[i + 1]->in == 0))
            {
                programs[i + 1]->input = (char*)realloc(programs[i + 1]->input, 2);
                programs[i + 1]->input[0] = 'c';
                programs[i + 1]->input[1] = '\0';
            }
        }

        if (programs[i]->output != NULL && (!strcmp(programs[i]->output, "n")))
        {
            if ((i + 1 < cnt_of_progs) && (programs[i]->pipeline == 1) && (programs[i]->out == 0))
            {
                programs[i + 1]->input = (char*)realloc(programs[i + 1]->input, 2);
                programs[i + 1]->input[0] = 'p';
                programs[i + 1]->input[1] = '\0';
            }
        }

        /*++programs[i]->number_of_arguments;
        programs[i]->arguments = (char**)realloc(programs[i]->arguments,
                    sizeof(char*) * programs[i]->number_of_arguments);
        programs[i]->arguments[programs[i]->number_of_arguments - 1] = NULL;*/
        /*
        if (programs[i]->pipeline)
        {
            if (i + 1 < cnt_of_progs)
            {
                if (programs[i + 1]->title != NULL)
                {
                    programs[i + 1]->input = 
                }
            }
        }*/
    }
    /*if (tmp_prog != NULL)
    {
        free(tmp_prog);
    }*/
    /*if (strlen(buffer) == 0)
    {
        free(buffer);
    }*/
    /*if (line != NULL)
    {
        free(line);
    }*/
    /*if (strlen(buffer) == 0)
    {
        free(buffer);
    }*/
}


void freeAllPrograms()
{
    int i;

    for (i = 0; i < cnt_of_progs; ++i)
    {
        freeProgram(programs[i]);     
    }
    free(programs);
}
			
void printPrograms()
{
    int i;
    int j;
    for (i = 0; i < cnt_of_progs; ++i)
    {
        j = 0;
        printf("title: %s\n", programs[i]->title);
        printf("in: %d\n", programs[i]->in);
        printf("out: %d\n", programs[i]->out);
        printf("input: %s\n", programs[i]->input);
        if (programs[i]->output != NULL)
        {
            printf("output: %s\n", programs[i]->output);
        }
        printf("arguments:\n");
        while (j < programs[i]->number_of_arguments)
        {
            printf("%d: %s\n", j, programs[i]->arguments[j]);
            ++j;
        }
        if (programs[i]->output_type == 1)
        {
            printf("output_type: rewrite\n");
        }
        if (programs[i]->output_type == 2)
        {
            printf("output_type: append\n");
        }
        printf("pipeline: %d\n", programs[i]->pipeline);
        
        printf("background: %d\n\n", programs[i]->background);
    }
}

void freeHistory()
{
    int i;

    for (i = 0; i < cnt_of_history; ++i)
    {
        if (hist[i].line != NULL)
        {
            free(hist[i].line);
        }
    }
    if (hist != NULL)
    {
        free(hist);
    }
}


void exitShell()
{
    
    if (line_present)
    {
        free(line);
        line_present = 0;
    }
    if (cur_dir_flg)
    {
        free(cur_dir);
        cur_dir_flg = 0;
    }

    freeAllPrograms();
    freeHistory();
    exit(EXIT_SUCCESS);
}

void execPrg(struct history_node *p)
{
    int fd[2];
    int i = 0;
    int p_fd = 0;
    pid_t s;

    if (p == NULL)
    {
        return;
    }
    if (p->err)
    {
        printf("ash: program isnt correct\n");
        return;
    }

    /*signal(SIGTTOU, ignoreSigInt);*/

    for (i = p->from; i <= p->to; ++i)
    {
        if (strcmp(programs[i]->title, "exit") == 0)
        {
            if (cur_dir_flg)
            {
                free(cur_dir);
                cur_dir_flg = 0;
            }
            ++cnt_of_history;

            exitShell();
        }
        
        else if (strcmp(programs[i]->title, "fg") == 0)
        {
            if (programs[i]->number_of_arguments == 1)
            {
                long long int num = atoi(programs[i]->arguments[0]);
                if (num > 0 && num <= cnt_of_jobs && hist[num - 1].status != 2)
                {
                    printf("PID: %d\n", hist[num - 1].pid_end);
                    int gid, tmp_status = 0, prev_in, prev_out/*, gid_old*//*, kappa = 0*/;
                    /*gid_old = getpgid(getpid());*/
                    prev_in = tcgetpgrp(0);
                    prev_out = tcgetpgrp(1);
		    signal(SIGTSTP, SIG_IGN);
                    gid = getpgid(hist[num - 1].pid_end);
                    setpgid(hist[num - 1].pid_end, gid);
                    tcsetpgrp(1, gid);
                    tcsetpgrp(0, gid);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTSTP, SIG_IGN);
                    kill(hist[num - 1].pid_end, SIGCONT);
                    /*signal(SIGTTOU, ignoreSigInt);*/
                    /*signal(SIGTSTP, ignoreSigInt);*/
                    /*kappa = */waitpid(hist[num - 1].pid_end, &tmp_status, WUNTRACED);
                
                    if (WIFEXITED(tmp_status) == 1)
                    {
                        hist[num - 1].status = 2;
                        process_status = WEXITSTATUS(tmp_status);
                    } 
                    else if (WIFSTOPPED(tmp_status) == 1)
                    {
                        hist[num - 1].status = 1;
                    }
                    else
                    {
                        hist[num - 1].status = 0;
                    }
                     
                    /*setpgid(getpid(), gid_old);*/
                    tcsetpgrp(1, prev_out);
                    tcsetpgrp(0, prev_in);
		    pid_to = -1;
                }
            }
            continue;
        }

        else if (strcmp(programs[i]->title, "jobs") == 0)
        {
            int j;
            int k = 1;
            for (j = 0; j < cnt_of_history; ++j)
            {
                if (hist[j].pid_end != 0)
                {
		            if (hist[j].pid_end != 1)
		            {
		                int tmp_status = 0, kappa = 0;
                        kappa = waitpid(hist[j].pid_end, &tmp_status, WUNTRACED | WNOHANG);
                        if (WIFSTOPPED(tmp_status) == 1)
                        {
                            /*printf("kappa\n");*/
                            hist[j].status = 1;
                        }
                        else if (kappa != 0)
                        {
                            hist[j].status = 2;
                        }
                    }
                    printf("[%d] ", k);
                    printf("PID: %d | ", hist[j].pid_end);
                    if (hist[j].status == 0)
                    {
                        printf("Running\n");
                    }
                    else if (hist[j].status == 1)
                    {
                        printf("Stopped\n");
                    }
                    else 
                    {
                        printf("Done\n");
                    }
                    int l;
                    for (l = hist[j].from; l <= hist[j].jobs_end; ++l)
                    {
                        printf("%s ", programs[l]->title);
                    }
                    printf("\n");
                    ++k;
                }
            }
            continue;
        }
        
        else if (strcmp(programs[i]->title, "cd") == 0)
        {
            if (programs[i]->number_of_arguments == 0)
            {
                programs[i]->arguments = (char**)malloc(sizeof(char*));
                programs[i]->arguments[0] = getcwd(NULL, 0);
            }
            else if (programs[i]->number_of_arguments > 1)
            {
                printf("cd: too nuch arguments for cd\n");
                continue;
            }
            if (chdir(programs[i]->arguments[0]) ==  -1)
            {
                printf("cd: cannot change path\n");
                continue;
            }
            continue;
        }

        else if (strcmp("pwd", programs[i]->title) == 0)
        {
            char *cur = getcwd(NULL, 0);
            printf("%s\n", cur);
            free(cur);
            continue;
        }
        
        if (strcmp(programs[i]->output, "n") == 0 && programs[i]->out == 0)
        {
            if (pipe(fd) < 0)
            {
                printf("ash: pipe failed\n");
                exit(EXIT_FAILURE);
            }
        }

        if ((s = fork()) < 0)
        {
            printf("ash: fork failed\n");
            exit(EXIT_FAILURE);
        }

        if (!s) /* try to exec */
        {
            int f;
            
            if (programs[i]->in)
            {
                f = open(programs[i]->input, O_RDONLY);

                if (f == -1)
                {
                    /* zakrivaem deskriptor */
                    close(0);
                }
                else
                {
                    dup2(f, 0);
                    close(f);
                }
            }
            else if (p_fd != 0 && programs[i]->in == 0 && strcmp(programs[i]->input, "p") == 0)
            {
                dup2(p_fd, 0);
                close(p_fd);
            }
            else if (programs[i]->in == 0 && strcmp(programs[i]->input, "c") == 0)
            {
                close(0);
            }

            if (programs[i]->out)
            {
                if (programs[i]->output_type == 1)
                {
                    f = open(programs[i]->output, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                }
                else
                {
                    f = open(programs[i]->output, O_CREAT | O_APPEND | O_WRONLY, 0666);
                }
                dup2(f, 1);
                close(f);
            }
            else if (programs[i]->out == 0 && strcmp(programs[i]->output, "n") == 0)
            {
                dup2(fd[1], 1);
                close(fd[0]);
                close(fd[1]);
            }

            int gid = getpgid(getpid());
            if (programs[i]->background)
            {
                signal(SIGTTIN, SIG_DFL);
                gid = getpgid(getpid());
                setpgid(getpid(), getpid());
            }
            
            char **buff = NULL;
            ++programs[i]->number_of_arguments;
            buff = (char**)malloc(sizeof(char*) 
                * (programs[i]->number_of_arguments + 1));
            int j = 1;
            buff[0] = programs[i]->title;
            for (; j < programs[i]->number_of_arguments; ++j)
            {
                buff[j] = (char*)malloc(strlen(programs[i]->arguments[j - 1]) + 1);
                strcpy(buff[j], programs[i]->arguments[j - 1]);
                /*buff[j] = programs[i]->arguments[j - 1];*/
            }
            buff[programs[i]->number_of_arguments] = NULL;
            ++programs[i]->number_of_arguments;
            
            execvp(programs[i]->title, buff);
            setpgid(getpid(), gid);
            process_status = 1;
            execlp("echo", "echo", "ash: error in execvp", NULL);
            for (j = 0; j < programs[i]->number_of_arguments; ++j)
            {
                free(buff[j]);
            }
            free(buff);
            exitShell();
            /*exit(EXIT_FAILURE);*/
        }

        if (programs[i]->out == 0 && strcmp(programs[i]->output, "n") == 0)
        {
            if (p_fd != 0)
            {
                close(p_fd);
            }
            p_fd = fd[0];
            close(fd[1]);
        }
        else 
        {
            if (p_fd != 0)
            {
                close(p_fd);
            }
            p_fd = 0;
        }
        if (programs[i]->background && programs[i]->pipeline != 1)
        {
            /*int tmp_status;
            pid_t kappa;*/
            ++cnt_of_jobs;
            signal(SIGTTIN, SIG_DFL);
	    signal(SIGTTOU, SIG_DFL);
	    pid_to = s;
            hist[cnt_of_history].jobs_end = i; 
            hist[cnt_of_history].pid_end = s;
            hist[cnt_of_history].status = 0;
        }
        
        else if (!programs[i]->background && programs[i]->pipeline == 0)
        {
            int tmp_status;

            signal(SIGINT, SIG_IGN);
            signal(SIGTSTP, SIG_IGN);
            waitpid(s, &tmp_status, WUNTRACED);
            pid_to = s;
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            /*signal(SIGINT, ignoreSigInt);
            signal(SIGTSTP, ignoreSigInt);*/

            /*int gid;
            gid = getpgid(getpid());*/

            if (WIFSTOPPED(tmp_status) == 1)
            {
                process_status = 2; /* stopped */
            }

            if (process_status == 2)
            {
                ++cnt_of_jobs;
                signal(SIGTTIN, SIG_DFL);
                hist[cnt_of_history].jobs_end = i; 
                hist[cnt_of_history].pid_end = s;
                hist[cnt_of_history].status = 1;
                /*int j;
                for (j = hist[cnt_of_history].from; j < i; ++j)
                {
                    printf("%s ", programs[j]->title);
                }
                printf("\n");*/

            }

            if (WIFEXITED(tmp_status) != 0) 
            {
                process_status = WEXITSTATUS(tmp_status);
                /*tcsetpgrp(0, gid);
                tcsetpgrp(1, gid);*/
            }
            else 
            {
                process_status = 1; 
                /*tcsetpgrp(0, gid);
                tcsetpgrp(1, gid);*/
            }
            
            pid_to = -1;
        }

        
    }
    return;
}



void printHistory()
{
    int i;

    if (!cnt_of_history)
    {
        printf("History is empty\n");
        return;
    }
    printf("History\n");

    for (i = 0; i < cnt_of_history; ++i)
    {
        printf("%d: %s \n", i + 1, hist[i].line);
        /*if (programs[i]->number_of_arguments > 0)
        {
            for (j = 0; j < programs[i]->number_of_arguments; ++j)
            {
                printf("%s ", programs[i]->arguments[j]);
            }
        }*/
    }
    return;
}

void ashLoop()
{
    signal(SIGTSTP, SIG_IGN);
    /*char *line = NULL;*/
	/*unsigned host_len = 0;*/

	clearWindow();
    /*cur_host = (char*)malloc(sizeof(char) * ASH_BUFF_SIZE);*/
    /*gethostname(cur_host, host_len);*/
	cur_host = getenv("HOSTNAME");
	if (cur_host == NULL)
	{
		cur_host = "ash";
	}

    cur_dir = getcwd(NULL, 0);
    cur_dir_flg = 1;
	cur_usr = getenv("USER");

    hist = (struct history_node*)malloc(sizeof(struct history_node));
    /*hist[0].line = (char*)malloc(sizeof(char));
    hist[0].line[0] = '\0';*/
	
    while (status) 
	{
        if (cur_dir_flg)
        {
            free(cur_dir);
            cur_dir_flg = 0;
        }

        if ((cur_dir = getcwd(NULL, 0)) != NULL)
        {
            cur_dir_flg = 1;
        }

        int gid;
        gid = getpgid(getpid());
        setpgid(getpid(), gid); 
        tcsetpgrp(0, gid);
        tcsetpgrp(1, gid);
		printf("\033[36;1m%s\033[0m@\033[34;1m%s\033[0m:\033[36m~%s\033[0m$ ", cur_usr, cur_host, cur_dir); 
        
        line = readLine();
        line_present = 1;
        
        if (eof_flg)
        {
            eof_flg = 0;
            exitShell();
        }
		/*printf(": %s\n\n", line);*/
        /*exit:*/    
        /*if (!strcmp(line, "exit"))
		{
	        status = 0;
            if (cur_dir_flg)
            {
                free(cur_dir);
                cur_dir_flg = 0;
            }*/
            /*if (line != NULL)
            {
                free(line);
            }*/

            /*continue;
		}*/
        
        if (!strcmp(line, "history"))
        {
            hist = (struct history_node*)realloc(hist, 
                sizeof(struct history_node) * (cnt_of_history + 1));
            hist[cnt_of_history].from = from;
            hist[cnt_of_history].to = from + 1;
            hist[cnt_of_history].line = (char*)malloc(sizeof(char) * (strlen(line) + 1));
            hist[cnt_of_history].err = 1;
            hist[cnt_of_history].jobs_end = 0;
            hist[cnt_of_history].pid_end = 0;
            hist[cnt_of_history].status = 0;
            strcpy(hist[cnt_of_history].line, line);

            ++cnt_of_history;
            from = from + 1;

            printHistory();
            
            if (line_present)
            {
                free(line);
                line_present = 0;
            }

            /*line = (char*)malloc(sizeof(char));
            line[0] = '\0';*/
            continue;
        }


        parseLine(line);

        if (line != NULL && strlen(line) != 0)
        {
            hist = (struct history_node*)realloc(hist, 
                sizeof(struct history_node) * (cnt_of_history + 1));
            hist[cnt_of_history].from = from;
            hist[cnt_of_history].to = from + tmp_cnt_of_progs - 1;
            hist[cnt_of_history].line = (char*)malloc(sizeof(char) * (strlen(line) + 1));
            hist[cnt_of_history].err = 0;
            hist[cnt_of_history].jobs_end = 0;
            hist[cnt_of_history].pid_end = 0;
            hist[cnt_of_history].status = 0;
            strcpy(hist[cnt_of_history].line, line);
            if (line_present)
            {
                free(line);
                line_present = 0;
            }

            
            if (error_flg)
            {
                hist[cnt_of_history].err = 1;
            }
            

                    
            if (error_flg)
            {
                printf("Syntax error: incorrect command\n");
            }
            else
            {
                execPrg(&hist[cnt_of_history]);
            }

            /*if (error_flg && cur_dir_flg)
            {
                free(cur_dir);
                cur_dir_flg = 0;
            }*/
	        /*cur_usr = getenv("USER");*/


            ++cnt_of_history;
            from = from + tmp_cnt_of_progs;
            tmp_cnt_of_progs = 0;

            error_flg = 0; 
            continue;
 
        }

        if (line_present)
        {
            free(line);
            line_present = 0;
        }
        
        /*if (line != NULL)
        {
            free(line);
        }*/


        tmp_cnt_of_progs = 0;
        
        if (error_flg)
        {
            printf("Syntax error: incorrect command\n");
        }
        
        error_flg = 0; 

        /*job.number_of_programs = tmp_cnt_of_progs;
        job.programs = programs + from;*/

                
    }
    /*if (line != NULL)
    {
        free(line);
    }*/
    /*if (line_present)
    {
        free(line);
        line_present = 0;
    }
    printPrograms();
    freeAllPrograms();
    freeHistory();*/
}
