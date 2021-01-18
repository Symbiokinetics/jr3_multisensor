#!/usr/bin/env python


import rospy
from std_msgs.msg import String
from geometry_msgs.msg import Twist
import struct

def pad_zeros(string, length):
    new_string = string;
    zz = length - len(string);
    if zz > 0:
        new_string = '0'*zz+string;
    return new_string;

def init_topics(frame):
    topics = {};
    for k, v in frame.items():
        for i in range(0,v['sensors']):
            topics[k+"_%d"%(i,)] = rospy.Publisher(k+"_%d"%(i,), Twist, queue_size=10);
    return topics

'''
gets frame from a file
structure:
dictionary with keys of sensors names
data[name] is a dictionary with 
    'timestamp' holds timestamp when the measurement was taken, 
    'sensors' holds number of sensors 1 or 2,
    'data' holds measurements [3 forces + 3 torques] * 'sensors'
'''
def get_frame(f):
    ret = {};
    while True:
        line = f.readline();
        if line.find('frame') >= 0:
            header = line.split(',');
            for i in range(0,int(header[2])):                                               
                line = f.readline();                                                        
                data=line.split(',');                                                       
                name = data[0];                                                             
                sensors_number = int(data[1]);                                              
                timestamp = pad_zeros(data[2],16);                                          
                timestamp = struct.unpack('!Q', timestamp.decode('hex'))[0];                

                ret[name] = {};
                ret[name]['timestamp'] = timestamp;
                ret[name]['sensors'] = sensors_number;
                ret[name]['values'] = [];

                for j in range(0,6*sensors_number):                 
                    v = pad_zeros(data[3+j],8);                     
                    force = struct.unpack('!f', v.decode('hex'))[0];
                    ret[name]['values'].append(force);

            break;

    return ret;

def publish(topics, frame):
    for k, v in frame.items():
        for i in range(0,v['sensors']):
            t = Twist();
            t.linear.x = v['values'][i*6];
            t.linear.y = v['values'][i*6+1];
            t.linear.z = v['values'][i*6+2];
            t.angular.x = v['values'][i*6+3];
            t.angular.y = v['values'][i*6+4];
            t.angular.z = v['values'][i*6+5];

            topics[k+"_%d" % (i,)].publish(t);

def talker():
    rospy.init_node('jr3_publisher', anonymous=True)
    #rate = rospy.Rate(10) # 10hz

    f = open("jr3_fifo","r");

    print("Running");

    frame = get_frame(f);    
    topics = init_topics(frame);
    while not rospy.is_shutdown():
        frame = get_frame(f);
        publish(topics,frame);
        #rate.sleep()

if __name__ == '__main__':
    try:
        talker()
    except rospy.ROSInterruptException:
        pass
