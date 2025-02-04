text = b"""xyzxyzxyzxyzxyzxyzxyzxyzxyz"""

threshold = .85 # probably no need to change this, threshold at what points should be annotated

from collections import Counter # to count frequency of every character
import matplotlib.pyplot as plt # graph plotting
from statistics import mean, stdev # to calculate the mean/stdev, not needed but it makes it more readable
import re, itertools # cleaning up input text


def ioc(txt): # calculate index of coincidence of given text
    counts = list(Counter(txt).values()) # Counter(txt) returns a dictionary of all characters and their frequencies
    numerator = sum([x*(x-1) for x in counts])
    return numerator/(len(txt)*(len(txt)-1))

def transpose(txt,n):
    transposed = [b"" for _ in range(n)] # creates list of r empty strings
    for i, c in enumerate(txt):
        transposed[i % n] += c.to_bytes(1,'big')
    return transposed
    
def cluster(data, maxgap): # this isn't needed for the test, this is solely to graph the data better. stolen from https://stackoverflow.com/questions/14783947/grouping-clustering-numbers-in-python
    groups = [[data[0]]]
    for x in data[1:]:
        if abs(x - groups[-1][-1]) <= maxgap:
            groups[-1].append(x)
        else:
            groups.append([x])
    return groups
    
def check(input,rnge): # run kullback text up to a given range
    xc, yc = list(range(1, rnge)), [] # xcoordinates, ycoordinates
    for x in range(1,rnge): # calculate IOC for each transposition length
        transpositions = transpose(input,x)
        avgioc = mean([ioc(y) for y in transpositions])
        yc.append(avgioc)
    plt.plot(xc,yc,'-bD',mfc="red") # plot points
    # code to label datapoints if the datapoint is above the average ioc for the text
    plt.ylim(top=max(yc)*1.05)
    iocmean = mean(yc)
    iocstdv = stdev(yc)
    spikes = []
    for i in range(rnge-1):
        if(((yc[i]-iocmean)/iocstdv) > threshold): # if the point deviates from the mean, treat it as a spike
            spikes.append(i)
    # removing clusters of spikes
    for k in cluster(spikes,1):
        m = max([yc[i] for i in k])
        # annotating (adding the number) above any spikes)
        plt.annotate(yc.index(m)+1,xy=(yc.index(m)+1,m),size=20,weight='bold',va="bottom",ha="center")
    plt.show()
    
check(text,int(len(text)/2)-1)