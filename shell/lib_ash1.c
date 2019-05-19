#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
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
	int input_flg;
	int output_flg;
};

struct job
{
	int background; /* 0 - background, 1 - foreground */
	struct program *programs;
	int number_of_programs;
	struct job *next;
};




/* GLOBALS */
int status = 1;
int cnt_of_progs = 0;
struct program **programs = NULL;


void ignoreSigInt(int sgn)
{
	signal(SIGINT, ignoreSigInt);
	return;
}

void clearWindow()
{
	start();
	clearscr();
	return;
}

char *getPathVariable(char *path)
{
	char *buffer = NULL;
	int i = 0;

	/* ? */
	if (path[0] == '?' && strlen(path) == 1)
	{
		/* last process status in last foreground job */
		return NULL; 
	}

	/* number */	
	int intpath;
	intpath = atoi(path);	
	if (intpath < ash_argc && intpath > 0)
	{
		buffer = ash_argv[intpath];
		return buffer;
	}

	/* USER */
    if (strcmp("USER", path) == 0)
	{
		buffer = (char*)malloc((strlen(getenv("USER")) + 1) * sizeof(char));
		strcpy(buffer, getenv("USER"));
		return buffer;
	}
		
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
		buffer = (char*)malloc(sizeof(char) * (strlen("ASHELL") + 1));
		strcpy(buffer, "ASHELL");
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
	
	/* # */
	if (strcmp("#", path) == 0)
	{
		buffer = (char*)malloc(sizeof(char) * 60); /* hardcode for int */
		sprintf(buffer, "%d", ash_argc - 1);
		return buffer; 
	}	

	/* HOME */
	if (strcmp("HOME", path) == 0)
	{
		buffer = (char*)malloc((strlen(getenv("HOME")) + 1) * sizeof(char));
		strcpy(buffer, getenv("HOME"));
		return buffer;
	}

	/* PWD */
	if (strcmp("PWD", path) == 0)
	{
		buffer = (char*)malloc((strlen(getenv("PWD")) + 1) * sizeof(char));
		strcpy(buffer, getenv("PWD"));
		return buffer;
	}
	
	/* if no correct PATH variable found */
	buffer = (char*)malloc(sizeof(char) * (strlen(path) + 2));
	buffer[0] = '$';
	for (i = 0; i < strlen(path); ++i)
	{
		buffer[i + 1] = path[i];
	}
	buffer[strlen(path) + 1] = '\0';
	return buffer;
}

/* reading line from shell and storing it in the buffer */
char *readLine()
{
	/* ignoring SIGINT */
	signal(SIGINT, ignoreSigInt);

	int buffersize = 0;
	int pos = 0;
    /*char *buffer = NULL;*/
	char *buffer = (char*)malloc(sizeof(char));
	int c;

	if (buffer == NULL)
	{
		fprintf(stderr, "ash: cannot allocate memory buffer\n");
		exit(EXIT_FAILURE);
	}
			
	while (1)
	{
		c = getchar();	
			
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


    
void parseLine(char *line)
{
	signal(SIGINT, ignoreSigInt);
	
	if (line == NULL)
	{
		fprintf(stderr, "ash: line is NULL in parseLine\n");
		exit(EXIT_FAILURE);
	}	

    if (!strcmp(line, "exit"))
    {
        status = 0;
        return;
    }

	int pos = 0;
	int len = 0;
	/*int cnt_of_input = 0;
	int cnt_of_output = 0;*/
	int i = 0;
    int buffer_size = ASH_BUFF_SIZE;
    int in = 0, out = 0;
	char *buffer = NULL;
	struct program *tmp_prog = NULL;


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
	
	len = strlen(line);
    /*printf("\nline: %s\n", line);*/

    /* adding ';' for the end of the line */
    if ((len >= 1) && (line[len - 1] != '&' || line[len - 1] != ';' || line[len - 1] != '|'))
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
	tmp_prog->input_flg = 0;
	tmp_prog->output_flg = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    buffer_size = 0;
                    i = 0;
                }
                else if (tmp_prog->title == NULL && in == 0 && out == 0 && strlen(buffer) != 0)
                {
                    buffer[i] = '\0';
                    ++i;
                    tmp_prog->title = (char*)realloc(tmp_prog->title, i * sizeof(char));
                    strcpy(tmp_prog->title, buffer);
                    free(buffer);
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    buffer_size = 0;
                    i = 0;
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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

                    out = 0;
                }
                in = 1;
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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

                    out = 0;
                }
                if (in == 0 && out == 0)
                {
                    free(tmp_prog->output);
                    tmp_prog->output = (char*)realloc(tmp_prog->output, sizeof(char) * 2);
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
                
                tmp_prog->background = 0;
                ++cnt_of_progs;
                programs = (struct program**)realloc(programs, 
                    sizeof(struct program*) * cnt_of_progs);
                
                programs[cnt_of_progs - 1] = tmp_prog;
                
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
		            tmp_prog->input_flg = 0;
		            tmp_prog->output_flg = 0;
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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;


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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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

                ++cnt_of_progs;
                programs = (struct program**)realloc(programs, 
                    sizeof(struct program*) * cnt_of_progs);
                
                programs[cnt_of_progs - 1] = tmp_prog;
                
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
		            tmp_prog->input_flg = 0;
		            tmp_prog->output_flg = 0;
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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                        buffer = (char*)malloc(sizeof(char));
                        buffer[0] = '\0';
                        i = 0;
                        buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                    buffer = (char*)malloc(sizeof(char));
                    buffer[0] = '\0';
                    i = 0;
                    buffer_size = 0;

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
                
                tmp_prog->background = 1;
                ++cnt_of_progs;
                programs = (struct program**)realloc(programs, 
                    sizeof(struct program*) * cnt_of_progs);
                
                programs[cnt_of_progs - 1] = tmp_prog;
                
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
		            tmp_prog->input_flg = 0;
		            tmp_prog->output_flg = 0;
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
    for (i = 0; i < cnt_of_progs; ++i)
    {
       /*  crutch 
        if (programs[i]->number_of_arguments == 1)
        {
            if (strlen(programs[i]->arguments[0]) == 0)
            {
                --programs[i]->number_of_arguments;
                free(programs[i]->arguments[0]);
            }
        }

         crutch 
        if (strlen(programs[i]->title) == 0)
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
        */
        if (!strcmp(programs[i]->output, "n"))
        {
            if ((i + 1 < cnt_of_progs) && (programs[i]->pipeline == 1))
            {
                programs[i + 1]->input = (char*)realloc(programs[i + 1]->input, 2);
                programs[i + 1]->input[0] = 'p';
                programs[i + 1]->input[1] = '\0';
            }
        }
    }
    /*if (tmp_prog != NULL)
    {
        free(tmp_prog);
    }*/
    if (buffer != NULL)
    {
        free(buffer);
    }
    if (line != NULL)
    {
        free(line);
    }
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
        printf("input: %s\n", programs[i]->input);
        printf("output: %s\n", programs[i]->output);
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
        
        printf("background: %d\n\n", programs[i]->background);
    }
}

void ashLoop()
{
    char *line = NULL;
	char *cur_dir = NULL;
	char *cur_usr = NULL;
	char *cur_host = NULL;
    /*unsigned host_len = 0;*/

	clearWindow();
	cur_usr = getenv("USER");
    /*cur_host = (char*)malloc(sizeof(char) * ASH_BUFF_SIZE);*/
    /*gethostname(cur_host, host_len);*/
	cur_host = getenv("HOSTNAME");
	if (cur_host == NULL)
	{
		cur_host = "ash";
	}
	
    while (status) 
	{
        cur_dir = getenv("PWD");
		printf("\033[36;1m%s\033[0m@\033[34;1m%s\033[0m:\033[36m~%s\033[0m$ ", cur_usr, cur_host, cur_dir); 
        
        line = readLine();
		printf(": %s\n\n", line);
        if (!strcmp(line, "exit"))
		{
			status = 0;
            continue;
		}

        parseLine(line);
        /*free(line);*/

    }
    
    if (line != NULL)
    {
        free(line);
    }
    printPrograms();
	/*while (status);*/
    /*free(cur_host);*/
    freeAllPrograms();
}
