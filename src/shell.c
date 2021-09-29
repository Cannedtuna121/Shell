#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <termios.h>
#include <dirent.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>

int line_buffer_start_size;
int token_buffer_size;
int small_token_buffer_size;
char *cdir;

char **cmdhist;
int cmdhistsize;
int cmdhistpos = -1;

int intSigCount = 0;
int executing = 0;

static struct termios old, cus; //used for terminal stuff

void chk_alloc_strar_mem(char ***string, int posused, int *buffersize, int chunksize)
{
  if (posused >= *buffersize - 1)
  {
    *buffersize += chunksize;
    *string = realloc(*string, *buffersize * sizeof(char*));
    if (!string) 
    {
     fprintf(stderr, "Failed to allocate memory.");
     exit(-1);
    }

  }

}

void chk_alloc_str_mem(char **string, int posused, int *buffersize, int chunksize)
{
  if (posused >= *buffersize - 1)
  {
    *buffersize += chunksize;
    *string = realloc(*string, *buffersize * sizeof(char));
    if (!string) 
    {
     fprintf(stderr, "Failed to allocate memory.");
     exit(-1);
    }

  }

}

void free_string_array(char ***string)
{
 int i = 0;
 char **string_arr = *string;
 while (string_arr[i] != NULL)
 {
  free(string_arr[i]);
  i++;
 }
 free(string_arr[i]);
 free(string_arr);

}

char** get_tokens(char *line);

int is_ready(int fd) 
{
 fd_set fdset;
 struct timeval timeout;
 int ret;
 FD_ZERO(&fdset);
 FD_SET(fd, &fdset);
 timeout.tv_sec = 0;
 timeout.tv_usec = 0;
 return select(1, &fdset, NULL, NULL, &timeout) == 1 ? 1 : 0;
}

void sigintHandler(int sig_num)
{
 if (!executing)
 {
  printf("\n^C\n");
  intSigCount++;
 }
}

char **get_files_in_dir(char* path)
{
 DIR *dr = opendir(path);
 
 if (dr == NULL)
 {
  return NULL;
 }

 struct dirent *de;
 char** files = malloc(64 * sizeof(char*));
 int bufsize = 64;
 int pos = 0;

 while((de=readdir(dr)) != NULL)
 {
  files[pos] = strdup(de->d_name);
  pos++;
  chk_alloc_strar_mem(&files, pos, &bufsize, 64);
 }

 files[pos] = NULL;
 closedir(dr);
 return files; 
}

char *get_tab_search(char* token)
{
 return strrchr(token, '/') + 1;
}

char *get_last_token(char* line)
{
 char **tokens = get_tokens(line);
 char *last;
 int pos = 0;
 while(tokens[pos] != NULL)
 {
  last = tokens[pos];
  pos++;
 }
 pos = 0;
 while(tokens[pos] != last)
 {
  free(tokens[pos]);
  pos++;
 }
 free(tokens);
 return last;
}

int swap_last_token(char** line, char* tok)
{
 char **tokens = get_tokens(*line);
 int pos = 0;
 while (tokens[pos] != NULL)
 {
  pos++;
 }
 tokens[pos-1] = tok;
 pos = 0;
 int bufsize = line_buffer_start_size;
 char* nline = malloc(bufsize * sizeof(char));
 int linepos = 0;
 while(tokens[pos] != NULL) 
 {
  int tokpos = 0;
  while(tokens[pos][tokpos] != '\0')
  {
   nline[linepos] = tokens[pos][tokpos];
   linepos++;
   tokpos++;
   chk_alloc_str_mem(&nline, linepos, &bufsize, line_buffer_start_size);
  }
  nline[linepos] = ' ';
  linepos++;
  chk_alloc_str_mem(&nline, linepos, &bufsize, line_buffer_start_size);
  pos++;
 }
 if (nline[linepos - 2] == '/')
 {
  nline[linepos - 1] = '\0';
 }
 else
 {
 nline[linepos] = '\0';
 }
 *line = nline;
 return bufsize;
}

char *get_search_dir(char *token)
{
 int index = strrchr(token, '/') - token;
 char* search_dir = malloc(index + 2);
 int pos = 0;
 while (pos <= index)
 {
  search_dir[pos] = token[pos];
  pos++;
 }
 search_dir[pos] = '\0';
 
 return search_dir;

 
}

int get_auto_line(char** line)
{
 if (*line[0] == '\0') //make sure there is actully something typed
  return strlen(*line);
 char *token = get_last_token(*line);

 char *search_string = strdup(token); //used to define the current search term
 char *search_dir = strdup(cdir); //used to define the current search directory context

 int search_type = 0; // will be set to one if it is a local search or 2 if it is a root search.


 if (strchr(token, '/') != NULL)
 {
  if (token[0] == '/')
  {
   search_dir = get_search_dir(token); //reasign the search dir to the directory the user is in
   search_type = 2;
  }
  else
  {
   char* sd = get_search_dir(token);
   char* tdir = malloc(strlen(cdir) + strlen(sd) + 2);
   int pos = 0;
   while (cdir[pos] != '\0')
   {
    tdir[pos] = cdir[pos];
    pos++;
   }
   tdir[pos] = '/';
   pos++;
   int endp = pos;
   while(sd[pos - endp] != '\0')
   {
    tdir[pos] = sd[pos - endp];
    pos++;
   }
   tdir[pos] = '\0';

   search_dir = tdir;
   search_type = 1;
  }

  search_string = get_tab_search(token); //reassign the search string when the user is looking into a different dir

 }

 char **files = get_files_in_dir(search_dir);
 if (files == NULL)
 {
  free(token);
  return strlen(*line);
 }
 int pos = 0;
 int matches = 0;
 char *match;
 while(files[pos] != NULL)
 {
  if (strncmp(search_string,files[pos], strlen(search_string)) == 0)
  {
   matches++;
   match = malloc(strlen(files[pos]) + 2);
   strcpy(match, files[pos]);
  }
  pos++;
 }
 if (matches > 1)
 {
  free(token);
  return strlen(*line);
 }
 else if (matches == 0)
 {
  char* dever = ":";
  char* syspaths = strdup(strtok(strdup(getenv("PATH")),dever));
  
  while(syspaths != NULL)
  {
   char **sysfiles = get_files_in_dir(syspaths);
   pos = 0;
   while(sysfiles[pos] != NULL)
   {
    if (strncmp(token,sysfiles[pos], strlen(token)) == 0)
    {
     matches++;
     if(matches > 1)
     {
      //printf("too many matches");
      return strlen(*line);
     }
     match = strdup(sysfiles[pos]);
    }
    pos++;
   }

   free(syspaths);
   free_string_array(&sysfiles);
   char* tmp = strtok(NULL, dever);
   if (tmp == NULL)
   {
    syspaths = NULL;
   }
   else
   {
    syspaths = strdup(tmp);
   }
  }
  if (matches == 1)
  {
   swap_last_token(line, match);
  }
  free(token);
  return strlen(*line);

  
 }
 else if (matches == 1)
 {
  char* tmp = malloc(strlen(search_dir) + strlen(match) + 2);
  strcpy(tmp, search_dir);
  strcat(strcat(tmp, "/"),match);
  struct stat path_stat;
  stat(tmp, &path_stat);

  if (strchr(match, ' ') != NULL) //surround with "" if there is a space
  {
   char* q1;
   q1[0] = '\"';
   q1[1] = '\0';
   match = strcat(strcat(q1, match), "\"");
  }

  if (access(tmp, X_OK) == 0 && S_ISREG(path_stat.st_mode))
  {
   if (strcmp(search_dir,cdir) == 0) //only add the ./ if in the current context
   {
    int tempms = 3 + strlen(match);
    char* tempm = malloc(tempms);
    tempm[0] = '.';
    tempm[1] = '/';
    tempm[2] = '\0';
  
    strcat(tempm, match);
    free(match);
    match = tempm;
   }
  }
  else if (S_ISDIR(path_stat.st_mode))
  {
   strcat(match,"/");
  }
  free(tmp);
  free(token);
 
  if (search_type == 1)
   swap_last_token(line, strcat(search_dir + strlen(cdir) + 1, match));
  else if (search_type == 2)
   swap_last_token(line, strcat(search_dir, match));
  else
   swap_last_token(line, match);
  return strlen(*line);
  }
}


int add_to_hist(char *line)
{
 for (int i = cmdhistsize-2; i >= 0;i--)
 {
  cmdhist[i+1] = cmdhist[i];
 }
  cmdhist[0] = line;
  return 0;
}

void change_termios(int echo)
{
 cus = old; /* make new settings same as old settings */
 cus.c_lflag &= ~ICANON; /* disable buffered i/o */
 if (echo) {
  cus.c_lflag |= ECHO; /* set echo mode */
 }
 else
 {
   cus.c_lflag &= ~ECHO; /* set no echo mode */
 }
 tcsetattr(0, TCSANOW, &cus); /* use these new terminal i/o settings now */
}


int cd(char *dir)
{

 if (dir == NULL)
 {
  fprintf(stderr, "no directory given");
 }
 else
 {
  if (chdir(dir) != 0)
  {
   perror("error:");
  }
  else
  {
   cdir = getcwd(cdir,2048 * sizeof(char));
  }
 }

}

int launch(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    if (execvp(args[0], args) == -1) {
      perror("error");
    }
    exit(-1);
  } else if (pid < 0) {
    perror("error");
  } else {
    do 
    {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

int pipe_launch(char **tokens)
{
 int pos = 0;
 int pipecount = 0;
 int count = 0;
 while(tokens[pos] != NULL)
 {
  if (strcmp(tokens[pos], "|") == 0)
  {
   if (count == 0)
   {
    fprintf(stderr, "%s", "No command given after pipe\n");
    return -1;
   }
   pipecount++;
   count = 0;
  }
  else
  {
   count++;
  }
  pos++;
 }
 
 if (count == 0)
 {
  fprintf(stderr, "%s", "No command given after pipe\n");
  return -1;
 }

 char ***args = malloc((pipecount+1) * sizeof(char**));
 for (int i = 0; i < pipecount+1;i++) 
 {
  args[i] = malloc(token_buffer_size * sizeof(char*));
 }
 pos = 0;
 int argi = 0;
 int tokeni = 0;
 while(tokens[pos] != NULL)
 {
  if (strcmp(tokens[pos], "|") == 0)
  {
   args[argi][tokeni] = NULL;
   
   argi++;
   tokeni = 0;
  }
  else
  {
   args[argi][tokeni] = tokens[pos];
   tokeni++;
  }
  pos++;
 }
 args[argi][tokeni] = NULL;

 int **pipes = malloc((pipecount+1) * sizeof(int*));
 
 for (int i = 0; i < pipecount+1;i++)
 {
  pipes[i] = malloc((2*sizeof(int)));
  pipe(pipes[i]);
 }

 int status; 
 pid_t wpid;
 pid_t *pids = malloc((pipecount+1)*sizeof(pid_t));
 for (int i = 0; i < pipecount+1;i++)
 {
  pids[i] = fork();
  if (pids[i] == 0)
  {
   if(i != pipecount)
   {
    if (dup2(pipes[i][1],1) < 0)
    {
     perror("dup2");
     exit(-1);
    }
   }
  
   if(i != 0)
   {
    if (dup2(pipes[i-1][0],0) < 0)
    {
     perror("dups");
     exit(-1);
    }
   }

   for (int j = 0; j < pipecount+1;j++)
   {
    for (int k = 1; k >= 0;k--)
    {
      if (close(pipes[j][k]) != 0)
       perror("Child couldn't close pipe");
    }
   }

   if (execvp(args[i][0], args[i]) < 0)
   {
    perror(args[i][0]);
    exit(-1);
   }
  }
 }


  for (int j = 0; j < pipecount+1;j++)
  {
   for (int k = 1; k >= 0;k--)
   {
    if (close(pipes[j][k]) != 0)
     perror("Couldnt close pipe");
   }
  }

  for (int j = 0; j < pipecount+1;j++)
  {
    do 
    {
      wpid = waitpid(pids[j], &status, WUNTRACED);
    }while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

 for (int i = 0; i < pipecount+1; i++)
 {
  free(args[i]);
  free(pipes[i]);
 }
 free(args);
 free(pipes);
 free(pids);
 return 1;
}

int redirect_launch(char** tokens)
{
 int redirectPos = -1;
 int pos = 0;
 int append = 0;
 int status;
 

 while(tokens[pos] != NULL)
 {
  if (strcmp(tokens[pos],">>") == 0 )
  {
   append = 1;
   if (redirectPos == -1)
    redirectPos = pos;
   else
    return -1;
  }
  else if (strcmp(tokens[pos],">") == 0)
  {
   if (redirectPos == -1)
    redirectPos = pos;
   else
    return -1; 
  }
  pos++;
 }
 
 if (redirectPos == -1 || tokens[redirectPos + 1] == NULL)
  return -1;

 int size = pos;
 pos = 0;
 char** args = malloc((redirectPos + 1) * sizeof(char**));
 char** extra = malloc((size - redirectPos - 1) * sizeof(char**));
 while(tokens[pos] != NULL)
 {
  if (pos < redirectPos)
  {
   args[pos] = strdup(tokens[pos]);
  }
  else if (pos == redirectPos)
  {
   args[pos] = NULL;
  }
  else if (pos > redirectPos + 1)
  {
   extra[pos - redirectPos - 2] = strdup(tokens[pos]);
  }
  pos++;
 }

 extra[pos - redirectPos - 2] = NULL;
 

 pid_t pid, wpid;

 pid = fork();
 //what the child will do
 if (pid == 0)
 {
  //get the file descripter and set up the file with its premmisions if needed
  int file_desc;
  if (append)
   file_desc = open(tokens[redirectPos + 1], O_WRONLY | O_APPEND | O_CREAT, 0644);
  else
   file_desc = open(tokens[redirectPos + 1], O_WRONLY | O_TRUNC | O_CREAT, 0644);

  
  if (dup2(file_desc,1) < 0)
  {
   perror("dup2");
   exit(-1);
  }

  close(file_desc);

  if (execvp(args[0], args) == -1)
  {
   perror("error");
  }
  exit(-1);

 }
 else if (pid < 0) //error checking
 {
  perror("fork error");
 }
 else
 {//what the parent will do
  do 
  {
   wpid = waitpid(pid, &status, WUNTRACED);
  }while (!WIFEXITED(status) && !WIFSIGNALED(status));
  //add the extra stuff to the file
  int file_desc = open(tokens[redirectPos + 1], O_WRONLY | O_APPEND | O_CREAT, 0644);
  pos = 0;
  while (extra[pos] != NULL)
  {
   write(file_desc, " ", 1);
   write(file_desc, extra[pos], strlen(extra[pos]));

   pos++;
  }

  free(args);
  free(extra);
  close(file_desc);

  return status;
 }
}

int execute(char **tokens) 
{

 if (tokens[0] == NULL)
 {
  return 1;
 }
 else if (strcmp(tokens[0], "cd") == 0)
 {
  cd(tokens[1]);
 }
 else if (strcmp(tokens[0], "exit") == 0)
 {
  exit(0);
 }
 else
 {
  int pos = 0;
  while(tokens[pos] != NULL)
  {
   if (strcmp(tokens[pos],"|") == 0)
   {
    return pipe_launch(tokens);
   }
   else if (strcmp(tokens[pos],">") == 0 || strcmp(tokens[pos], ">>") == 0)
   {
    return redirect_launch(tokens);
   }
   pos++;
  }
  
  launch(tokens);
 }
 

}

char** get_tokens(char *line)
{
 int bufsize = token_buffer_size;
 char **tokens = malloc(bufsize * sizeof(char*));
 char *token;

 if (!tokens)
 {
  fprintf(stderr, "Failed to allocate memory.");
  exit(-1);
 }
 

 int c;
 int line_position = 0;
 int token_position = 0;
 int tokens_position = 0;
 int small_token_bufsize = small_token_buffer_size;
 token = malloc(sizeof(char) * small_token_bufsize);
 token[0] = '\0';
 int in_quotations = 0;
 do 
  {
  c = line[line_position];
  line_position++; //makes it so we can easily check the next charecter without having to remeber to add a + 1 to line position when we check
  if (c == '"') //noticing quotations
  {
   in_quotations = !in_quotations;
  } //spliting to a new token if there is a space
  else if ((c == ' ' && !in_quotations) || c == '\0')
  {
   if (strcmp(token,"") != 0 && token[0]!='\0')
   {
    token[token_position] = '\0';
    tokens[tokens_position] = token;
    tokens_position++;
    small_token_bufsize = small_token_buffer_size;
    token = malloc(sizeof(char) * small_token_bufsize);
    token_position = 0;
   }
  }
  else if (c == '>' && !in_quotations) //recognizing redirection as tokens
  {
   token[token_position] = '\0';
   if (strcmp(token, "") != 0)
   {
    tokens[tokens_position] = token;
    tokens_position++;
    chk_alloc_strar_mem(&tokens, tokens_position, &bufsize, token_buffer_size);
   }

   small_token_bufsize = small_token_buffer_size;
   token = malloc(sizeof(char) * small_token_bufsize);
   token_position = 0;
   
   if (line[line_position] == '>')
   {
    token[0] = '>';
    token[1] = '>';
    token[2] = '\0';
    line_position++;
   }
   else
   {
    token[0] = '>';
    token[1] = '\0';   
   }
   tokens[tokens_position] = token;
   tokens_position++;
   token = malloc(sizeof(char) * small_token_bufsize);
   token[0] = '\0';

  }
  else if (c == '|' && !in_quotations) //recognizing pipes as tokens
  {
   token[token_position] = '\0';
   if (strcmp(token, "") != 0)
   {
    tokens[tokens_position] = token;
    tokens_position++;
    chk_alloc_strar_mem(&tokens, tokens_position, &bufsize, token_buffer_size);
   }
   else
   {
    free(token);
   }
   small_token_bufsize = small_token_buffer_size;
   token = malloc(sizeof(char) * small_token_bufsize);
   token_position = 0;
   
   token[0] = '|';
   token[1] = '\0';   
   tokens[tokens_position] = token;
   tokens_position++;
   token = malloc(sizeof(char) * small_token_bufsize);
   token[0] = '\0';
  }
  else if (!in_quotations && c == '!' && line[line_position] == '!')
  {
   token[token_position] = '\0';
   if (strcmp(token, "") != 0)
   {
    tokens[tokens_position] = token;
    tokens_position++;
    chk_alloc_strar_mem(&tokens, tokens_position, &bufsize, token_buffer_size);
   }

   //reget the old tokens and put them inside the new ones if we can
   if (cmdhist[1] != NULL && strstr(cmdhist[1], "!") == NULL)
   {
    char** old_tokens = get_tokens(cmdhist[1]);
    int old_pos = 0;
    while(old_tokens[old_pos] != NULL)
    {
    tokens[tokens_position] = old_tokens[old_pos];
    tokens_position++;
    old_pos++;
    }
   }

   line_position++; //move past the !!

  }
  else //adding the chars into the tokens
  {
   token[token_position] = c;
   token_position++;
  }
  
  //realocating memory if needed
  chk_alloc_str_mem(&token, token_position, &small_token_bufsize, small_token_buffer_size);

  chk_alloc_strar_mem(&tokens, tokens_position, &bufsize, token_buffer_size);


  
 }while(c != '\0'); 
 //putting a null terminator on the tokens array
 tokens[tokens_position] = NULL;
 free(token);


 return tokens;
 
}

char* read_line()
{
  int line_buffer_size = line_buffer_start_size;
  char *buffer = malloc(sizeof(char) * line_buffer_size);
  int c; 
  int shift = 0;

  int position = 0;
  while(1)
  {

   //return nothing if ctrl c was pressed
   if(intSigCount > 0)
   {
    free(buffer);
    return "";
   }

   //check if there is nothing and continue (to prevent blocking on getchar())
   if (is_ready(0) == 0)
   {
    usleep(1000); //reduce cpu usage
    continue;
   }

   c = getchar();

      
   
   if (c == EOF || c == '\n')
   {
    buffer[position] = '\0';
    printf("\n");
    return buffer;
   }
   else if(c == 127) //if they clicked backspace
   {
    if (position > 0)
    {
     if (shift == 0)
     {
      printf("\b \b");
      position--;
      buffer[position] = '-';
     }
     else if (shift > 0 && position - shift != 0)
     {
     buffer[position] = '\0';
     char* bufcpy = strdup(buffer);
     for (int i = 0; i < strlen(bufcpy);i++)
     {
      if (i < position - shift -1)
      {
       buffer[i] = bufcpy[i];
      }
      else if (i > position - shift - 1)
      {
       buffer[i-1] = bufcpy[i];
      }
      else
      {
      
      }
     }
     position--;
     chk_alloc_str_mem(&buffer, position, &line_buffer_size, line_buffer_start_size);
     buffer[position] = '\0';
     //printf("\n%s\n", buffer);
     free(bufcpy);
     
     printf("%c", '\b');
     for (int i = position - shift; i < position;i++)
     {
      printf("%c", buffer[i]);
     }
     printf("%s", " \b");
     for (int i = position - shift; i < position;i++)
     {
      printf("%c", '\b');
     }

     }
    } 
   }
   else if(c == 9) //checking for tab
   {
    buffer[position] = '\0';
    for (int i = 0; i < position;i++)
    {
     printf("\b \b");
    }
    line_buffer_size = get_auto_line(&buffer);
    position = strlen(buffer); 
    chk_alloc_str_mem(&buffer, position, &line_buffer_size, line_buffer_start_size);
    printf("%s", buffer); 
   }
   else if(c == 27) //checking for arrow keys
   {
    int checker = getchar();
    if (checker == 91) //checking to see if an arrow key was pressed
    {
     int direction = getchar();
     if (direction == 65) //up was pressed
     {
      if (cmdhist[cmdhistpos+1] != NULL)
      {
       cmdhistpos++;
       //clear the text on screen and the buffer
       while (position > 0)
       {
        printf("\b \b");
        position--;
        buffer[position] = '\0';
       }
       //get rid of the memory of buffer
       free(buffer);
       //get memory for the newly sized buffer
       buffer = malloc(sizeof(char) * (strlen(cmdhist[cmdhistpos]) + 1));
       if (!buffer) 
       {
        fprintf(stderr, "Failed to allocate memory.");
        exit(-1);
       }
       //copy the history call into the buffer
       for(int i = 0; i < strlen(cmdhist[cmdhistpos]); i++)
       {
        buffer[i] = cmdhist[cmdhistpos][i];
       }
       position = strlen(cmdhist[cmdhistpos]);
       line_buffer_size = position;
       printf("%s", cmdhist[cmdhistpos]);
      }
     }
     else if (direction == 66) //down was pressed
     {
      if (cmdhistpos - 1 >= -1)
      {
       cmdhistpos--;
       //clear the text on screen and the buffer
       while (position > 0)
       {
        printf("\b \b");
        position--;
        buffer[position] = '-';
       }
       //get rid of the memory of buffer
       free(buffer);
       if (cmdhistpos != -1)
       {
        //get memory for the newly sized buffer
        buffer = malloc(sizeof(char) * strlen(cmdhist[cmdhistpos]));
        if (!buffer) 
        {
         fprintf(stderr, "Failed to allocate memory.");
         exit(-1);
        }
        //copy the history call into the buffer
        for(int i = 0; i < strlen(cmdhist[cmdhistpos]); i++)
        {
         buffer[i] = cmdhist[cmdhistpos][i];
        }
        position = strlen(cmdhist[cmdhistpos]);
        line_buffer_size = position;
        printf("%s", cmdhist[cmdhistpos]);
       }
       else
       {
        buffer = malloc(sizeof(char) * line_buffer_start_size);
       }
      }
     }
     else if (direction == 67) //right was pressed
     {
      shift--;
      if (shift < 0)
       shift = 0;
      else
       printf("\x1b[1C"); 
     }
     else if (direction == 68) //left was pressed
     {
      shift++;
      if (shift > position)
       shift = position;
      else
       printf("\b");
     }
    }
   }
   else
   {
    if (shift == 0)
    {
     buffer[position] = c;
     printf("%c", c);
     
     position++;
    }
    else
    {
     buffer[position] = '\0';
     char* bufcpy = strdup(buffer);
     for (int i = 0; i < strlen(bufcpy) + 1;i++)
     {
      if (i < position - shift)
      {
       buffer[i] = bufcpy[i];
      }
      else if (i > position - shift)
      {
       buffer[i] = bufcpy[i-1];
      }
      else
      {
       buffer[i] = c;
      }
     }
     position++;
     chk_alloc_str_mem(&buffer, position, &line_buffer_size, line_buffer_start_size);
     buffer[position] = '\0';
     //printf("%s", buffer);
     free(bufcpy);
    
     for (int i = position - shift -1; i < position;i++)
     {
      printf("%c", buffer[i]);
     }
     for (int i = position - shift; i < position;i++)
     {
      printf("%c", '\b');
     }
    }
   }
   //realocate buffer
   chk_alloc_str_mem(&buffer, position, &line_buffer_size, line_buffer_start_size);
   //make sure that all the printfs actully display
   fflush(stdout); 
  }
 
}



void shell_loop()
{
 do {
  printf(cdir);
  printf(">");
  fflush(stdout);
  char *line = read_line();
  if (intSigCount == 0)
  {
  add_to_hist(line);
  cmdhistpos = -1;
  char **tokens = get_tokens(line);
  
  tcsetattr(0, TCSANOW, &old);
  executing = 1;
  int status = execute(tokens);
  executing = 0;
  change_termios(0);

  free_string_array(&tokens);
  }
  else
  {
   intSigCount = 0;
  }
 }while(1);

}


int main(int argc, char **argv)
{
 //Set up the buffer sizes
 line_buffer_start_size = 256;
 token_buffer_size = 64;
 small_token_buffer_size = 64;
 
 //Make the signal handler the sigintHandler function
 signal(SIGINT, sigintHandler);
 
 cdir = malloc(2048 * sizeof(char));
 cdir = getcwd(cdir,2048 * sizeof(char));

 //make the terminal give raw inputs
 tcgetattr(0, &old);
 change_termios(0); 

 
 //set up cmd history
 cmdhistsize = 1024; 
 cmdhist = malloc(sizeof(char*) * cmdhistsize);
 if (!cmdhist) 
 {
  fprintf(stderr, "Failed to allocate memory.");
  exit(-1);
 }
 //make sure everything in the history starts out null (make sure old memory values dont show up)
 for (int i = 0; i < cmdhistsize;i++)
 {
  cmdhist[i] = NULL;
 }
 
 //start the shell loop
 shell_loop();
 
 //set the terminal back to the way it was
 tcsetattr(0, TCSANOW, &old);

  
 return 0;
}
