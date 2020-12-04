#include <vector>
#include <fstream>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

struct process
{
    int id;
    string name;
    process(int pid, string cmdName)
    {
        id = pid;
        name = cmdName;
    }
};

vector<process> processes;

/*---------------------------------------
TODO:
    handling background processes
    
-----------------------------------------*/

vector<string> split(string str, string token)
{
    vector<string> result;
    while (str.size())
    {
        int index = str.find(token);
        if (index != string::npos)
        {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)
                result.push_back(str);
        }
        else
        {
            result.push_back(str);
            str = "";
        }
    }
    return result;
}

int getFileSize(string fileName)
{
    ifstream file(fileName, ios::binary | ios::ate);
    int fileSize = file.tellg();
    file.close();
    return fileSize;
}

void execute(string command)
{

    vector<string> tokens = split(command, " ");
    bool done = false;
    for (int i = 0; i < tokens.size(); i++) //This loop looks for strings within quotes and merges
    {
        if (tokens[i][0] == '\'' || tokens[i][0] == '\"')
        {
            for (int j = i + 1; j < tokens.size(); j++)
            {
                tokens[i] = tokens[i] + " " + tokens[j];
                string token = tokens[j];
                tokens.erase(tokens.begin() + j);
                j--;
                if (token[token.length() - 1] == '\'' || token[token.length() - 1] == '\"')
                {
                    break;
                }
            }
            tokens[i] = tokens[i].substr(1, tokens[i].length() - 2);
        }
        if(tokens[i] == "&"){return;}
    }
    int tokenSize = tokens.size();
    if (tokens[0].compare("") == 0) //handles beginning whitespace
    {
        tokenSize--;
        tokens.erase(tokens.begin());
    }
    if (tokens[tokens.size() - 1].compare("") == 0) //handles end whitespace
    {
        tokenSize--;
        tokens.erase(tokens.end());
    }
    char *cmd = (char *)tokens[0].c_str(); //pulls command

    //-------------------------------------------------------------------------------------------

    vector<string> args = tokens; //process args for I/O redirection

    for (int i = 0; i < args.size(); i++) //output to file
    {

        
        if (args[i] == "<") //input from file
        {
            string filename = args[i + 1];
            int fd = open(filename.c_str(), O_RDONLY);
            
            dup2(fd,0);
            args.erase(args.begin() + i);
            args.erase(args.begin() + i + 1);
            // for (int j = 0; j < inputArgs.size(); j++)
            // {
            //     args.push_back(inputArgs[j]);
            // }
        }

        if (args[i] == ">")
        {
            string filename = args[i + 1];
            int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_RDONLY | O_TRUNC);
            dup2(fd, 1);
            args.erase(args.begin() + i);
            args.erase(args.begin() + i + 1);
        }

    }

    //-------------------------------------------------------------------------------------------

    char *argsArr[args.size() + 1]; //transfer from vector to char[]
    for (int i = 0; i < args.size(); i++)
    {
        argsArr[i] = (char *)args[i].c_str();
    }
    argsArr[args.size()] = NULL; //add ending null

    processes.push_back(process(getpid(), cmd));
    for (int i = 0; i < processes.size(); i++)
    {
        //cout << "Process id: " << processes[i].id << endl;
        waitpid(processes[i].id, 0, WNOHANG);
    }

    if (args.size() != 0)
    { //exec args
        execvp(cmd, argsArr);
    }
    else
    { //exec no args
        execlp(cmd, cmd, NULL);
    }
}

void initializePipes(string input)
{
    vector<string> pipeCommands = split(input, "|");
    bool singleFound = false;
    for (int i = 0; i < pipeCommands.size() - 1; i++) //handle single quotes
    {
        if (pipeCommands[i].find("\'") != string::npos)
        { //look for single quote
            for (int j = i + 1; j < pipeCommands.size(); j++)
            { //look for next quote
                if (pipeCommands[j].find("\'") != string::npos)
                {
                    pipeCommands[i] = pipeCommands[i] + "|" + pipeCommands[j];
                    pipeCommands.erase(pipeCommands.begin() + j);
                    singleFound = true;
                    break;
                }
            }
        }
        if (true == singleFound)
        {
            break;
        }
    }
    bool doubleFound = false;
    for (int i = 0; i < pipeCommands.size() - 1; i++) //handle double quotes
    {
        if (pipeCommands[i].find("\"") != string::npos)
        { //look for single quote
            for (int j = i + 1; j < pipeCommands.size(); j++)
            { //look for next quote
                if (pipeCommands[j].find("\"") != string::npos)
                {
                    pipeCommands[i] = pipeCommands[i] + "|" + pipeCommands[j];
                    pipeCommands.erase(pipeCommands.begin() + j);
                    doubleFound = true;
                    break;
                }
            }
        }
        if (true == doubleFound)
        {
            break;
        }
    }

    for (int i = 0; i < pipeCommands.size(); i++)
    {
        // set up the pipe
        int fd[2];
        pipe(fd);
        if (!fork())
        { // child process
            /* redirect output to the next level unless this is the last level */
            if (i < pipeCommands.size() - 1)
            {
                dup2(fd[1], 1); // redirect STDOUT to fd[1], so that it can write to the other side be closed
            }
            execute(pipeCommands[i]); // this is where you execute the command, you NEED to write this function
        }
        else
        {
            if (i == pipeCommands.size() - 1)
            { // wait only for the last child
                wait(0);
            }
            dup2(fd[0], 0); // now redirect the input for the next loop iteration
            close(fd[1]);   // fd [1] MUST be closed, otherwise the next level will wait
        }
    }
}

int main()
{
    int saveIn;
    saveIn = dup(0);
    vector<string> inputs;
    while (true)
    {
        dup2(saveIn, 0); //save default stdin

        char directory[100];
        getcwd(directory, sizeof(directory));

        cout << "austin@Otterbox:~" << directory << "$ "; //print a prompt
        string inputline;
        getline(cin, inputline); //get a line from standard input
        inputs.push_back(inputline);

        if (inputline == string("exit")) //check for exiting shell
        {
            cout << "Bye!! End of shell" << endl;
            break;
        }
        else if (inputline.substr(0, 2) == "cd") //check for directory change
        {
            char *moveTo = (char *)inputline.substr(3, inputline.length()).c_str(); //pull directory to move to
            if (chdir(moveTo) == -1)
            {
                cout << "No such file or directory" << endl;
            }
        }
        else if (inputline.substr(0, 4) == "jobs")
        {
            cout << "listing jobs" << endl;
            for (int i = 0; i < processes.size(); i++)
            {
                cout << "Process id: " << processes[i].id << "              " << processes[i].name << endl;
            }
        }
        else //commands
        {
            int pid = fork();
            if (pid == 0)
            { //child process
                // preparing the input command for execution
                bool pipeFound = false;
                for (char c : inputline)
                {
                    if (c == '|')
                    {
                        pipeFound = true;
                        initializePipes(inputline);
                        break;
                    }
                }
                if (!pipeFound)
                {
                    execute(inputline);
                }
            }
            else
            {
                wait(0); //parent waits for child process
            }
        }
    }
}
