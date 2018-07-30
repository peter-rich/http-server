# http-server

## Author: Zhanfu Yang.    yang1676@purdue.edu

### This is a web server with pthread's poolSlave or multiple processes. Which is the homework in Purdue University. Besides, I have implemented the bash function and cgi-bin which can run bash or cgi-file in the cgi-bin/ directory.

## How to use:

### You can clone this project in your web server.  First:

`git clone https://github.com/peter-rich/http-server.git./run.sh `
                                                               
`cd http-server/                                                 `
                                                                
`make clean                                                     `
                                                                
`make                                                            `
                                                                
`chmod +777 run.sh                                               `
                                                                 
`./run.sh                                                        ` 

### In the directory httpServer/

Then 'http-root/' is the '/' directory.

### If you wan to run some bash online 

Like piazza or finger. you need to change some line

#### In the finger:

Change the finger with the environment_variables with Finger=<Actually file path>

#### In the jj:

`Change the cgi-script/jj.c  search for the line www.zhanfuyang.com:50011 change it to your actual DNS and port`
                                                                                                        
`make clean                                                                                                    `
                                                                                                              
`make                                                                                                          `


# For More Detail about testing the HTTP-Server,   Click on the link: http://www.zhanfuyang.com:50011/
