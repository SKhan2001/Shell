Shell.c
shell.c is a C-based application that mimics the bash terminal. It allows users to execute programs, move jobs to the foreground or background, kill jobs, and navigate through directories using the ls and ls -l commands.

How it works
The shell.c application creates a shell environment that allows users to enter commands. The shell reads in a command from the user, parses the command, and then executes the appropriate program or command.

shell.c has several built-in commands that can be executed. These include:

cd: Change the current directory.
pwd: Print the current working directory.
echo: Print a message to the console.
exit: Exit the shell.
In addition to the built-in commands, shell.c can also execute external programs. Users can enter the name of an external program and the shell will execute it.

Jobs
shell.c has a built-in job control system that allows users to move jobs to the foreground or background and to kill jobs. When a user executes a program, it runs as a job in the background. Users can view a list of all running jobs using the jobs command. To move a job to the foreground, users can use the fg command. To move a job to the background, users can use the bg command. To kill a job, users can use the kill command followed by the job number.

Navigation
shell.c allows users to navigate through directories using the ls and ls -l commands. The ls command lists the contents of the current directory, while the ls -l command lists the contents of the current directory in long format. Users can also change the current directory using the cd command.

Conclusion
shell.c is a C-based application that mimics the bash terminal. It allows users to execute programs, move jobs to the foreground or background, kill jobs, and navigate through directories using the ls and ls -l commands. The application has a built-in job control system that allows users to manage running jobs. The navigation system allows users to navigate through directories and change the current directory using the cd command.

I worked on this project with 2 other group members
