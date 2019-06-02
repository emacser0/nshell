#include "nshell.h"

int main(int argc, char *argv[])
{
  if(argc!=4)
  {
		printf("Usage : %s <IP> <port> <UID>\n", argv[0]);
		exit(1);
	}
  RainbowSocketTCP socket_tcp;
  if(RainbowSocketTCP_Initialize
     (
       &socket_tcp,
       RainbowProtocolFamily_IPv4,
       RainbowAddressFamily_IPv4
     ) < 0)
  {
    perror("RainbowSocketTCP_Initialize: ");
    exit(1);
  }
  Call(socket_tcp, SetPort, atoi(argv[2]));
  if(Call(socket_tcp, Connect, argv[1], strlen(argv[1]) < 0))
  {
    perror("RainbowSocketTCP_Connect: ");
    Call(socket_tcp, Destroy);
    exit(1);
  }
  RainbowFileStream * socket_file_stream = Call(socket_tcp, GetFileStream);
  if(CallP(socket_file_stream, Printf, "0, %s\n", argv[3]) <= 0)
  {
    printf("Failed to Transfer Flag, UID\n");
    Call(socket_tcp, Destroy);
    exit(1);
  }

  int output_fd = make_tempfile(), stdout_backup, flag=1;

  char output_buf[OUTPUT_BUF_MAX_SIZE],
       prompt_string[PROMPT_STRING_MAX_SIZE];

  Tokenizer tokenizer;
  tokenizer_init(&tokenizer);

  History history;
  history_open(&history);

  InterpretContext icontext;
  interpret_context_init(&icontext, &history, &tokenizer);

  FILE *output_file = fdopen(output_fd, "r+");
  setvbuf(output_file, NULL, _IOLBF, 0);

  char cmd[CMD_BUF_MAX_SIZE];
  while(flag)
  {
    get_prompt(prompt_string);
    printf("%s", prompt_string);
    size_t prompt_length = strlen(prompt_string);
    prompt_string[prompt_length++] = '\n';
    prompt_string[prompt_length] = '\0';
    int plength = 0;
    char * next_ptr = strchr(prompt_string, '\n');
    while(next_ptr != NULL)
    {
      plength += 1;
      next_ptr = strchr(next_ptr + 1, '\n');
    }

    if(CallP(socket_file_stream, Printf, "%d\n", plength) <= 0)
    {
      printf("An Error occured during plength tranfer\n");
      plength = 0;
      break;
    }

    if(CallP(socket_file_stream, Printf, "%s", prompt_string) < prompt_length)
    {
      printf("An Error occured during prompt string tranfer\n");
      break;
    }

    if(CallP(socket_file_stream, Gets, cmd, CMD_BUF_MAX_SIZE - 1) == NULL)
    {
      printf("An Error occured while receiving command string\n");
      break;
    }
    printf("%s\n", cmd);
    int cmd_len = strlen(cmd);

    tokenizer_tokenize(&tokenizer, cmd, cmd_len);
    history_update(&history, cmd, cmd_len);

    swapout_stdout(&output_fd, &stdout_backup);

    off_t prev = lseek(STDOUT_FILENO, 0, SEEK_CUR);
    if(tokenizer_get_count(&tokenizer) > 0)
    {
      flag = interpret(&icontext);
    }
    puts("");

    off_t current = lseek(STDOUT_FILENO, 0, SEEK_CUR);
    off_t offlen = current - prev;

    lseek(STDOUT_FILENO, prev, SEEK_SET);

    swapin_stdout(&output_fd, &stdout_backup);

    RainbowVector result;
    RainbowVector_Initialize(&result, sizeof(RainbowString));

    RainbowString string;
    char * start_message = "\x1B[36mRecvClient : Command Result\x1B[0m\n";
    RainbowString_Initialize(&string, start_message, strlen(start_message));
    Call(result, PushBack, &string);
    while(offlen  > 0 && fgets(output_buf, OUTPUT_BUF_MAX_SIZE, output_file) != NULL)
    {
      RainbowString string;
      RainbowString_Initialize(&string, output_buf, strlen(output_buf));
      Call(result, PushBack, &string);
      offlen -= strlen(output_buf);
    }


    size_t vlength = Call(result, Size);
    CallP(socket_file_stream, Printf, "%d\n", vlength);
    for(int i = 0; i < vlength; ++i)
    {
      RainbowString * string = Call(result, At, i);
      const char * cstring = CallP(string, CStr);
      printf("%s", cstring);
      CallP(socket_file_stream, Printf, "%s", cstring);
      CallP(string, Destroy);
    }
    Call(result, Destroy);

    fflush(stdout);
    lseek(output_fd, 0, SEEK_END);
  }

  tokenizer_destroy(&tokenizer);
  fclose(output_file);
  history_close(&history);
  remove_tempfile_all();

  Call(socket_tcp, Destroy);
}
