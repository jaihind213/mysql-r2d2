mysql-r2d2
==========

A simple message queue storage engine for MySQL.  Inspired by r2d2 from star wars :)


mysql> show engines;

+--------+---------+---------------------------------------------------------+--------------+----+------------+

| Engine | Support | Comment                                                 | Transactions | XA | Savepoints |

+--------+---------+---------------------------------------------------------+--------------+----+------------+

| r2d2   | YES     | Interface with message queues.(may the force be with u) | NO           | NO | NO         |

+------- +---------+---------------------------------------------------------+--------------+----+------------+

High level design
-----------------

https://drive.google.com/file/d/0B71-jD__Xq3qVWZRVHhkb2l5MEk/edit?usp=sharing

PLEASE ZOOM INTO THE IMAGE.

How to work with this engine ?
-----------------------------

(Step-0) compile the java producer library. refer to https://github.com/jaihind213/r2d2-java

(Step-1) compile the storage engine(refer build instructions i.e. `build_me.txt` -  currently tested on  mac10.7.5). 
         copy 'ha_r2d2.so' to directory shown by command `show variables like '%plugin%';`

(Step-2) start mysqld server

(Step-3) open mysql prompt & run the following: `INSTALL PLUGIN r2d2 SONAME 'ha_r2d2.so';`

         check the logs. the engine should have been registered successfully. you will also find a class not found exception! 

(Step-4) To Fix the exception: add following variable to my.cnf `r2d2_jvm_arguments_var="-Djava.class.path=<path_to_r2d2-java.jar>"`
         Then restart mysql. check the logs and make sure the classnotFound exception has gone.

(Step-5) create table. please note the mandatory column 'payload' should be present. 

	create table foo(payload varchar(213))engine=r2d2  CONNECTION='producer_type:CONSOLE;dest_type:TOPIC;dest_name:bar;key1:value1';

	Note: The connection string specifies the following:
	
		(a) producer_type:CONSOLE
                    The type of producer, i.e. here we have console producer, producing to (stdout).
                    For valid values, refer to enum `MessengerType` of the r2d2-java lib.  https://github.com/jaihind213/r2d2-java
		   
                    if the java lib, has an implementation for say KAFKA,then this storage engine can deliver to KAFKA. So the more 
                    implementations of producers/messengers we have in the java lib, the power of this engine grows.
                   
 		(b) dest_type:TOPIC 
                    The type of destination, valid values are TOPIC or QUEUE
		(c) dest_name:bar
                    The name of destination
	        (d) Other configuration params specified as key/value pairs i.e. key1 with value1
 
        Note: The delimiters for parameter tuple is SEMI_COLON and within the tuple its COLON
        Note: these delimiters are defined in `ConfigConstants.java` of the r2d2-java lib.  https://github.com/jaihind213/r2d2-java  


(step-6) insert into foo values ('message body payload');
	
	The stdout of mysql should have the following:

        Invoking on object calling java method from c. !
        INFO [2014-02-26 19:47:09,789] [Thread-2] (ConsoleProducer.java:31) - CONSOLE PRODUCER sending message: message body payload ,destination: bar, hashcode:1554207397
 


And thats it, R2D2 has delivered your message !   
--------------------------------------------------

How about Transactions?
-----------------------
Its possible. by means of what i call the LEGO pattern :) 

The 2 building blocks - blackhole & r2d2.

have master / slave setup.

on master , place block 1 i.e. have your table with engine set as 'blackhole'
on slave , place block 2 i.e have your table with engine set as 'r2d2'

when you commit on master, the binlog records the transaction, this comes to slave where the table is of engine r2d2,

depending on the type of producer set in connection string, the r2d2 engine will receive the statement & table will deliver the message. 

if error occurs, => replication breaks , lets say due to message queue server going on. once back up. simply run `start slave` command on slave mysql server.

How does r2d2 send messages/connect to a message queue?
----------------------------------------------

The r2d2-java library has the neccessary classes to connect to various message queues. (used JNI) 

https://github.com/jaihind213/r2d2-java

To implement an interface to a message queue like kafka, you need to implement the 'Messenger' interface and register it as an 'MessengerType' enum value.

compile the r2d2-java project and specify the jar as jvm argument to mysql variable 'r2d2_jvm_arguments_var' in my.cnf

you will notice the value for producer_type in CONNECTION String is the same you add as enum in 'MessengerType.java' 

Test cases:
-----------
https://docs.google.com/spreadsheet/ccc?key=0Ar1-jD__Xq3qdHdRVlk4aG1PTWZIUGlXcWZia0w0ekE#gid=1


