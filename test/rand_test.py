#!/usr/bin/python
from collections import MutableMapping
import string
import random
import bmemcached       
import sys
import os

class RandomDict(MutableMapping):
    def __init__(self, *args, **kwargs):
        """ Create RandomDict object with contents specified by arguments.
        Any argument
        :param *args:       dictionaries whose contents get added to this dict
        :param **kwargs:    key, value pairs will be added to this dict
        """
        # mapping of keys to array positions
        self.keys = {}
        self.values = []
        self.last_index = -1

        self.update(*args, **kwargs)

    def __setitem__(self, key, val):
        if key in self.keys:
            i = self.keys[key]
        else:
            self.last_index += 1
            i = self.last_index

        self.values.append((key, val))
        self.keys[key] = i
    
    def __delitem__(self, key):
        if not key in self.keys:
            raise KeyError

        # index of item to delete is i
        i = self.keys[key]
        # last item in values array is
        move_key, move_val = self.values.pop()

        if i != self.last_index:
            # we move the last item into its location
            self.values[i] = (move_key, move_val)
            self.keys[move_key] = i
        # else it was the last item and we just throw
        # it away

        # shorten array of values
        self.last_index -= 1
        # remove deleted key
        del self.keys[key]
    
    def __getitem__(self, key):
        if not key in self.keys:
            raise KeyError

        i = self.keys[key]
        return self.values[i][1]

    def __iter__(self):
        return iter(self.keys)

    def __len__(self):
        return self.last_index + 1

    def random_key(self):
        """ Return a random key from this dictionary in O(1) time """
        if len(self) == 0:
            raise KeyError("RandomDict is empty")
        
        i = random.randint(0, self.last_index)
        return self.values[i][0]

    def random_value(self):
        """ Return a random value from this dictionary in O(1) time """
        return self[self.random_key()]

    def random_item(self):
        """ Return a random key-value pair from this dictionary in O(1) time """
        k = self.random_key()
        return k, self[k]
                
def random_generator(size=6, chars=string.ascii_uppercase + string.digits):
    return ''.join(random.choice(chars) for x in range(size))

my_dict = {}
len(sys.argv)    
if len(sys.argv) > 1:
    testcount=int(sys.argv[1])
else:
    testcount=100000


for i in range (testcount)  :
    key=random_generator(size=6)
    value=random_generator()
    my_dict[key]=value
    
print"Dict Ready Start testing"

rand_dict=RandomDict(my_dict);
if len(sys.argv) > 2:
    port=int(sys.argv[2])
else:
    port=5000    
server="127.0.0.1:" + str(port)
print server
client = bmemcached.Client(server,username=None, password=None, socket_timeout=10000)
#client = bmemcached.Client('127.0.0.1:2041',username=None, password=None, socket_timeout=10000)

setpass=0
setfailed=0
n=0
for k in my_dict.keys():
    n+=1
    result = client.set(k,my_dict[k])
    if  result is not True:
        setfailed+=1
        #print "pass : ",setpass," failed for ", k
    else:
        setpass+=1
    if n % 1000 == 0:
        _=os.system("clear")
        print " Set> Pass : ",setpass,"Failed : ",setfailed
        
        
_=os.system("clear")
print " Set> Pass : ",setpass,"Failed : ",setfailed

getpass=0
mismatch=0
n=0
for i in range(testcount):
    n+=1
    key=rand_dict.random_key()
    x=client.get(key)
    if x != None:
        #print "Failed for ",key
        getpass+=1
    elif x is not my_dict[key]:
        #print "Mismtach, for", key," is ", x, "should be ",my_dict[key]
        mismatch+=1
    if n % 1000 == 0:
        _=os.system("clear")
        print " Set> Pass : ",setpass,"Failed : ",setfailed
        
        
_=os.system("clear")
print " Set> Pass : ",setpass,"Failed : ",setfailed
print " Get> Pass : ",getpass,"Failed : ",mismatch
client.disconnect_all()
