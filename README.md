mysql-r2d2
==========

A simple message queue storage engine for MySQL.  Inspired by r2d2 from star wars :)


mysql> show engines;

+--------+---------+---------------------------------------------------------+--------------+----+------------+

| Engine | Support | Comment                                                 | Transactions | XA | Savepoints |

+--------+---------+---------------------------------------------------------+--------------+----+------------+

| r2d2   | YES     | Interface with message queues.(may the force be with u) | NO           | NO | NO         |

+------- +---------+---------------------------------------------------------+--------------+----+------------+


How to work with this engine ?
-----------------------------

(Step-1) compile the storage engine(refer build instructions). copy 'ha_r2d2.so' to directory shown by command `show variables like '%plugin%';`


(Step-2) add following variable to my.cnf `r2d2_jvm_arguments_var="-Djava.class.path=<path_to_r2d2-java.jar>"`


(Step-3) start mysqld server


(Step-4) open mysql prompt & run the following: `INSTALL PLUGIN r2d2 SONAME 'ha_r2d2.so';`


(Step-5) create table. please note the mandatory column 'payload' should be present. 

	create table foo(payload varchar(213))engine=r2d2  CONNECTION='producer_type:BLACKHOLE;dest_type:TOPIC;dest_name:bar;key1:value1';

	Note: The connection string specifies the following:
	
		(a) producer_type:BLACKHOLE
                    The type of producer, i.e. here we have blackhole producer, producing to blackhole(stdout), valid values are `BLACKHOLE` for now (TODO: Add KAFKA)
 		(b) dest_type:TOPIC 
                    The type of destination, valid values are TOPIC or QUEUE
		(c) dest_name:bar
                    The name of destinatin
	        (d) Other configuration params specified as key/value pairs i.e. key1 with value1
 
        Note: The delimiters for parameter tuple is SEMI_COLON and within the tuple its COLON


(step-6) insert into foo values ('message body payload');
	
	The stdout of mysql should have the following:

        Invoking on object calling java method from c. !
        INFO [2014-02-26 19:47:09,789] [Thread-2] (BlackHoleProducer.java:31) - BLACK HOLE PRODUCER sending message: message body payload ,destination: bar, hashcode:1554207397
 

--------------------------------------------------
And thats it, R2D2 has delivered your message !   
--------------------------------------------------

The r2d2-java library has the neccessary classes to connect to various message queues. 
https://github.com/jaihind213/r2d2-java

To implement an interface to a message queue like kafka, you need to implement the 'Messenger' interface and register it as an 'MessengerType' enum value.

compile the r2d2-java project and specify the jar as jvm argument to mysql variable 'r2d2_jvm_arguments_var' in my.cnf

you will notice the value for producer_type in CONNECTION String is the same you add as enum in 'MessengerType.java' 
