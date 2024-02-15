var Statistics=new(function(){this.min=function(values){return Math.min.apply(Math,values);}
this.max=function(values){return Math.max.apply(Math,values);}
this.sum=function(values){return values.length?values.reduce(function(a,b){return a+b;}):0;}
this.mean=function(values){return this.sum(values)/values.length;}
this.weightedMean=function(valuesWithWeights){let totalWeight=0;let sum=0;for(const entry of valuesWithWeights){totalWeight+=entry.weight;sum+=entry.value*entry.weight;}
return sum/totalWeight;}
this.median=function(values){return values.sort(function(a,b){return a-b;})[Math.floor(values.length/2)];}
this.squareSum=function(values){return values.length?values.reduce(function(sum,value){return sum+value*value;},0):0;}
this.sampleStandardDeviation=function(numberOfSamples,sum,squareSum){if(numberOfSamples<2)
return 0;return Math.sqrt(squareSum/(numberOfSamples-1)-sum*sum/(numberOfSamples-1)/numberOfSamples);}
this.supportedConfidenceIntervalProbabilities=function(){const supportedProbabilities=[];for(const probability in tDistributionByOneSidedProbability)
supportedProbabilities.push(oneSidedToTwoSidedProbability(probability).toFixed(2));return supportedProbabilities;}
this.supportedOneSideTTestProbabilities=function(){return Object.keys(tDistributionByOneSidedProbability);}
this.confidenceIntervalDelta=function(probability,numberOfSamples,sum,squareSum){var oneSidedProbability=twoSidedToOneSidedProbability(probability);if(!(oneSidedProbability in tDistributionByOneSidedProbability)){throw'We only support '+this.supportedConfidenceIntervalProbabilities().map(function(probability)
{return probability*100+'%';}).join(', ')+' confidence intervals.';}
if(numberOfSamples-2<0)
return NaN;return this.minimumTForOneSidedProbability(oneSidedProbability,numberOfSamples-1)*this.sampleStandardDeviation(numberOfSamples,sum,squareSum)/Math.sqrt(numberOfSamples);}
this.confidenceInterval=function(values,probability){var sum=this.sum(values);var mean=sum/values.length;var delta=this.confidenceIntervalDelta(probability||0.95,values.length,sum,this.squareSum(values));return[mean-delta,mean+delta];}
this.testWelchsT=function(values1,values2,probability){return this.computeWelchsT(values1,0,values1.length,values2,0,values2.length,probability).significantlyDifferent;}
function sampleMeanAndVarianceFromMultipleSamples(samples){let sum=0;let squareSum=0;let size=0;for(const sample of samples){sum+=sample.sum;squareSum+=sample.squareSum;size+=sample.sampleSize;}
const mean=sum/size;const unbiasedSampleVariance=(squareSum-sum*sum/size)/(size-1);return{mean,variance:unbiasedSampleVariance,size,degreesOfFreedom:size-1,}}
this.probabilityRangeForWelchsT=function(values1,values2){var result=this.computeWelchsT(values1,0,values1.length,values2,0,values2.length);return this._determinetwoSidedProbabilityBoundaryForWelchsT(result.t,result.degreesOfFreedom);}
this.probabilityRangeForWelchsTForMultipleSamples=function(sampleSet1,sampleSet2){const stat1=sampleMeanAndVarianceFromMultipleSamples(sampleSet1);const stat2=sampleMeanAndVarianceFromMultipleSamples(sampleSet2);const combinedT=this._computeWelchsTFromStatistics(stat1,stat2);return this._determinetwoSidedProbabilityBoundaryForWelchsT(combinedT.t,combinedT.degreesOfFreedom)}
this._determinetwoSidedProbabilityBoundaryForWelchsT=function(t,degreesOfFreedom){if(isNaN(t)||isNaN(degreesOfFreedom))
return{t:NaN,degreesOfFreedom:NaN,range:[null,null]};let lowerBound=null;let upperBound=null;for(const probability in tDistributionByOneSidedProbability){const twoSidedProbability=oneSidedToTwoSidedProbability(probability);if(t>this.minimumTForOneSidedProbability(probability,Math.round(degreesOfFreedom)))
lowerBound=twoSidedProbability;else if(lowerBound){upperBound=twoSidedProbability;break;}}
return{t,degreesOfFreedom,range:[lowerBound,upperBound]};};this.computeWelchsT=function(values1,startIndex1,length1,values2,startIndex2,length2,probability){const stat1=sampleMeanAndVarianceForValues(values1,startIndex1,length1);const stat2=sampleMeanAndVarianceForValues(values2,startIndex2,length2);const{t,degreesOfFreedom}=this._computeWelchsTFromStatistics(stat1,stat2);const minT=this.minimumTForOneSidedProbability(twoSidedToOneSidedProbability(probability||0.8),Math.round(degreesOfFreedom));return{t,degreesOfFreedom,significantlyDifferent:t>minT};};this._computeWelchsTFromStatistics=function(stat1,stat2){const sumOfSampleVarianceOverSampleSize=stat1.variance/stat1.size+stat2.variance/stat2.size;const t=Math.abs((stat1.mean-stat2.mean)/Math.sqrt(sumOfSampleVarianceOverSampleSize)); const degreesOfFreedom=sumOfSampleVarianceOverSampleSize*sumOfSampleVarianceOverSampleSize/
(stat1.variance*stat1.variance/stat1.size/stat1.size/stat1.degreesOfFreedom
+stat2.variance*stat2.variance/stat2.size/stat2.size/stat2.degreesOfFreedom);return{t,degreesOfFreedom};}
this.findRangesForChangeDetectionsWithWelchsTTest=function(values,segmentations,oneSidedPossibility=0.99){if(!values.length)
return[];const selectedRanges=[];const twoSidedFromOneSidedPossibility=2*oneSidedPossibility-1;for(let i=1;i+2<segmentations.length;i+=2){let found=false;const previousMean=segmentations[i].value;const currentMean=segmentations[i+1].value;console.assert(currentMean!=previousMean);const currentChangePoint=segmentations[i].seriesIndex;const start=segmentations[i-1].seriesIndex;const end=segmentations[i+2].seriesIndex;for(let leftEdge=currentChangePoint-2,rightEdge=currentChangePoint+2;leftEdge>=start&&rightEdge<=end;leftEdge--,rightEdge++){const result=this.computeWelchsT(values,leftEdge,currentChangePoint-leftEdge,values,currentChangePoint,rightEdge-currentChangePoint,twoSidedFromOneSidedPossibility);if(result.significantlyDifferent){selectedRanges.push({startIndex:leftEdge,endIndex:rightEdge-1,segmentationStartValue:previousMean,segmentationEndValue:currentMean});found=true;break;}}
if(!found&&Statistics.debuggingTestingRangeNomination)
console.log('Failed to find a testing range at',currentChangePoint,'changing from',previousMean,'to',currentMean);}
return selectedRanges;};function sampleMeanAndVarianceForValues(values,startIndex,length){var sum=0;for(var i=0;i<length;i++)
sum+=values[startIndex+i];var squareSum=0;for(var i=0;i<length;i++)
squareSum+=values[startIndex+i]*values[startIndex+i];var sampleMean=sum/length;var unbiasedSampleVariance=(squareSum-sum*sum/length)/(length-1);return{mean:sampleMean,variance:unbiasedSampleVariance,size:length,degreesOfFreedom:length-1,}}
this.movingAverage=function(values,backwardWindowSize,forwardWindowSize){var averages=new Array(values.length);for(var i=0;i<values.length;i++){var sum=0;var count=0;for(var j=i-backwardWindowSize;j<=i+forwardWindowSize;j++){if(j>=0&&j<values.length){sum+=values[j];count++;}}
averages[i]=sum/count;}
return averages;}
this.cumulativeMovingAverage=function(values){var averages=new Array(values.length);var sum=0;for(var i=0;i<values.length;i++){sum+=values[i];averages[i]=sum/(i+1);}
return averages;}
this.exponentialMovingAverage=function(values,smoothingFactor){var averages=new Array(values.length);var movingAverage=values[0];averages[0]=movingAverage;for(var i=1;i<values.length;i++){movingAverage=smoothingFactor*values[i]+(1-smoothingFactor)*movingAverage;averages[i]=movingAverage;}
return averages;}
this.segmentTimeSeriesGreedyWithStudentsTTest=function(values,minLength){if(values.length<2)
return[0];var segments=new Array;recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values,0,values.length,minLength,segments);segments.push(values.length);return segments;}
this.debuggingSegmentation=false;this.segmentTimeSeriesByMaximizingSchwarzCriterion=function(values,segmentCountWeight,gridSize){var gridLength=gridSize||500;var totalSegmentation=[0];for(var gridCount=0;gridCount<Math.ceil(values.length/gridLength);gridCount++){var gridValues=values.slice(gridCount*gridLength,(gridCount+1)*gridLength);var segmentation=splitIntoSegmentsUntilGoodEnough(gridValues,segmentCountWeight);if(Statistics.debuggingSegmentation)
console.log('grid='+gridCount,segmentation);for(var i=1;i<segmentation.length-1;i++)
totalSegmentation.push(gridCount*gridLength+segmentation[i]);}
if(Statistics.debuggingSegmentation)
console.log('Final Segmentation',totalSegmentation);totalSegmentation.push(values.length);return totalSegmentation;}
function recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values,startIndex,length,minLength,segments){var tMax=0;var argTMax=null;for(var i=1;i<length-1;i++){var firstLength=i;var secondLength=length-i;if(firstLength<minLength||secondLength<minLength)
continue;var result=Statistics.computeWelchsT(values,startIndex,firstLength,values,startIndex+i,secondLength,0.9);if(result.significantlyDifferent&&result.t>tMax){tMax=result.t;argTMax=i;}}
if(!tMax){segments.push(startIndex);return;}
recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values,startIndex,argTMax,minLength,segments);recursivelySplitIntoTwoSegmentsAtMaxTIfSignificantlyDifferent(values,startIndex+argTMax,length-argTMax,minLength,segments);}
this.minimumTForOneSidedProbability=function(probability,degreesOfFreedom){if(degreesOfFreedom<1||isNaN(degreesOfFreedom))
return NaN;const tDistributionTableForProbability=tDistributionByOneSidedProbability[probability];if(degreesOfFreedom<=tDistributionTableForProbability.probabilityToTValue.length)
return tDistributionTableForProbability.probabilityToTValue[degreesOfFreedom-1];const tValuesSortedByProbability=tDistributionTableForProbability.tValuesSortedByProbability;let low=0;let high=tValuesSortedByProbability.length;while(low<high){const mid=low+Math.floor((high-low)/2);const entry=tValuesSortedByProbability[mid];if(degreesOfFreedom<=entry.maxDF)
high=mid;else
low=mid+1;}
return tValuesSortedByProbability[low].value;}
const tDistributionByOneSidedProbability={0.9:{probabilityToTValue:[3.0777,1.8856,1.6377,1.5332,1.4759,1.4398,1.4149,1.3968,1.383,1.3722,1.3634,1.3562,1.3502,1.345,1.3406,1.3368,1.3334,1.3304,1.3277,1.3253,1.3232,1.3212,1.3195,1.3178,1.3163,1.315,1.3137,1.3125,1.3114,1.3104,1.3095,1.3086,1.3077,1.307,1.3062,1.3055,1.3049,1.3042,1.3036,1.3031,1.3025,1.302,1.3016,1.3011,1.3006,1.3002,1.2998,1.2994,1.2991,1.2987,1.2984,1.298,1.2977,1.2974,1.2971,1.2969,1.2966,1.2963,1.2961,1.2958,1.2956,1.2954,1.2951,1.2949,1.2947,1.2945,1.2943,1.2941,1.2939,1.2938,1.2936,1.2934,1.2933,1.2931,1.2929,1.2928,1.2926,1.2925,1.2924,1.2922,1.2921,1.292,1.2918,1.2917,1.2916,1.2915,1.2914,1.2912,1.2911,1.291,1.2909,1.2908,1.2907,1.2906,1.2905,1.2904,1.2903,1.2903,1.2902,1.2901],tValuesSortedByProbability:[{maxDF:101,value:1.2900},{maxDF:102,value:1.2899},{maxDF:103,value:1.2898},{maxDF:105,value:1.2897},{maxDF:106,value:1.2896},{maxDF:107,value:1.2895},{maxDF:109,value:1.2894},{maxDF:110,value:1.2893},{maxDF:112,value:1.2892},{maxDF:113,value:1.2891},{maxDF:115,value:1.2890},{maxDF:116,value:1.2889},{maxDF:118,value:1.2888},{maxDF:119,value:1.2887},{maxDF:121,value:1.2886},{maxDF:123,value:1.2885},{maxDF:125,value:1.2884},{maxDF:127,value:1.2883},{maxDF:128,value:1.2882},{maxDF:130,value:1.2881},{maxDF:132,value:1.2880},{maxDF:135,value:1.2879},{maxDF:137,value:1.2878},{maxDF:139,value:1.2877},{maxDF:141,value:1.2876},{maxDF:144,value:1.2875},{maxDF:146,value:1.2874},{maxDF:149,value:1.2873},{maxDF:151,value:1.2872},{maxDF:154,value:1.2871},{maxDF:157,value:1.2870},{maxDF:160,value:1.2869},{maxDF:163,value:1.2868},{maxDF:166,value:1.2867},{maxDF:170,value:1.2866},{maxDF:173,value:1.2865},{maxDF:177,value:1.2864},{maxDF:180,value:1.2863},{maxDF:184,value:1.2862},{maxDF:188,value:1.2861},{maxDF:193,value:1.2860},{maxDF:197,value:1.2859},{maxDF:202,value:1.2858},{maxDF:207,value:1.2857},{maxDF:212,value:1.2856},{maxDF:217,value:1.2855},{maxDF:223,value:1.2854},{maxDF:229,value:1.2853},{maxDF:235,value:1.2852},{maxDF:242,value:1.2851},{maxDF:249,value:1.2850},{maxDF:257,value:1.2849},{maxDF:265,value:1.2848},{maxDF:273,value:1.2847},{maxDF:283,value:1.2846},{maxDF:292,value:1.2845},{maxDF:303,value:1.2844},{maxDF:314,value:1.2843},{maxDF:326,value:1.2842},{maxDF:339,value:1.2841},{maxDF:353,value:1.2840},{maxDF:369,value:1.2839},{maxDF:385,value:1.2838},{maxDF:404,value:1.2837},{maxDF:424,value:1.2836},{maxDF:446,value:1.2835},{maxDF:471,value:1.2834},{maxDF:499,value:1.2833},{maxDF:530,value:1.2832},{maxDF:565,value:1.2831},{maxDF:606,value:1.2830},{maxDF:652,value:1.2829},{maxDF:707,value:1.2828},{maxDF:771,value:1.2827},{maxDF:848,value:1.2826},{maxDF:942,value:1.2825},{maxDF:1060,value:1.2824},{maxDF:1212,value:1.2823},{maxDF:1415,value:1.2822},{maxDF:1699,value:1.2821},{maxDF:2125,value:1.2820},{maxDF:2837,value:1.2819},{maxDF:4266,value:1.2818},{maxDF:8601,value:1.2817},{maxDF:Infinity,value:1.2816}]},0.95:{probabilityToTValue:[6.3138,2.92,2.3534,2.1318,2.015,1.9432,1.8946,1.8595,1.8331,1.8125,1.7959,1.7823,1.7709,1.7613,1.7531,1.7459,1.7396,1.7341,1.7291,1.7247,1.7207,1.7171,1.7139,1.7109,1.7081,1.7056,1.7033,1.7011,1.6991,1.6973,1.6955,1.6939,1.6924,1.6909,1.6896,1.6883,1.6871,1.686,1.6849,1.6839,1.6829,1.682,1.6811,1.6802,1.6794,1.6787,1.6779,1.6772,1.6766,1.6759,1.6753,1.6747,1.6741,1.6736,1.673,1.6725,1.672,1.6716,1.6711,1.6706,1.6702,1.6698,1.6694,1.669,1.6686,1.6683,1.6679,1.6676,1.6672,1.6669,1.6666,1.6663,1.666,1.6657,1.6654,1.6652,1.6649,1.6646,1.6644,1.6641,1.6639,1.6636,1.6634,1.6632,1.663,1.6628,1.6626,1.6624,1.6622,1.662,1.6618,1.6616,1.6614,1.6612,1.6611,1.6609,1.6607,1.6606,1.6604,1.6602,1.6601,1.6599,1.6598,1.6596,1.6595,1.6594,1.6592,1.6591,1.659,1.6588,1.6587,1.6586,1.6585,1.6583,1.6582,1.6581,1.658,1.6579,1.6578,1.6577],tValuesSortedByProbability:[{maxDF:121,value:1.6575},{maxDF:122,value:1.6574},{maxDF:123,value:1.6573},{maxDF:124,value:1.6572},{maxDF:125,value:1.6571},{maxDF:126,value:1.6570},{maxDF:127,value:1.6569},{maxDF:129,value:1.6568},{maxDF:130,value:1.6567},{maxDF:131,value:1.6566},{maxDF:132,value:1.6565},{maxDF:133,value:1.6564},{maxDF:134,value:1.6563},{maxDF:135,value:1.6562},{maxDF:137,value:1.6561},{maxDF:138,value:1.6560},{maxDF:139,value:1.6559},{maxDF:140,value:1.6558},{maxDF:142,value:1.6557},{maxDF:143,value:1.6556},{maxDF:144,value:1.6555},{maxDF:146,value:1.6554},{maxDF:147,value:1.6553},{maxDF:148,value:1.6552},{maxDF:150,value:1.6551},{maxDF:151,value:1.6550},{maxDF:153,value:1.6549},{maxDF:154,value:1.6548},{maxDF:156,value:1.6547},{maxDF:158,value:1.6546},{maxDF:159,value:1.6545},{maxDF:161,value:1.6544},{maxDF:163,value:1.6543},{maxDF:164,value:1.6542},{maxDF:166,value:1.6541},{maxDF:168,value:1.6540},{maxDF:170,value:1.6539},{maxDF:172,value:1.6538},{maxDF:174,value:1.6537},{maxDF:176,value:1.6536},{maxDF:178,value:1.6535},{maxDF:180,value:1.6534},{maxDF:182,value:1.6533},{maxDF:184,value:1.6532},{maxDF:186,value:1.6531},{maxDF:189,value:1.6530},{maxDF:191,value:1.6529},{maxDF:193,value:1.6528},{maxDF:196,value:1.6527},{maxDF:198,value:1.6526},{maxDF:201,value:1.6525},{maxDF:204,value:1.6524},{maxDF:206,value:1.6523},{maxDF:209,value:1.6522},{maxDF:212,value:1.6521},{maxDF:215,value:1.6520},{maxDF:218,value:1.6519},{maxDF:221,value:1.6518},{maxDF:225,value:1.6517},{maxDF:228,value:1.6516},{maxDF:231,value:1.6515},{maxDF:235,value:1.6514},{maxDF:239,value:1.6513},{maxDF:242,value:1.6512},{maxDF:246,value:1.6511},{maxDF:250,value:1.6510},{maxDF:255,value:1.6509},{maxDF:259,value:1.6508},{maxDF:263,value:1.6507},{maxDF:268,value:1.6506},{maxDF:273,value:1.6505},{maxDF:278,value:1.6504},{maxDF:283,value:1.6503},{maxDF:288,value:1.6502},{maxDF:294,value:1.6501},{maxDF:299,value:1.6500},{maxDF:305,value:1.6499},{maxDF:312,value:1.6498},{maxDF:318,value:1.6497},{maxDF:325,value:1.6496},{maxDF:332,value:1.6495},{maxDF:339,value:1.6494},{maxDF:347,value:1.6493},{maxDF:355,value:1.6492},{maxDF:364,value:1.6491},{maxDF:372,value:1.6490},{maxDF:382,value:1.6489},{maxDF:392,value:1.6488},{maxDF:402,value:1.6487},{maxDF:413,value:1.6486},{maxDF:424,value:1.6485},{maxDF:436,value:1.6484},{maxDF:449,value:1.6483},{maxDF:463,value:1.6482},{maxDF:477,value:1.6481},{maxDF:493,value:1.6480},{maxDF:509,value:1.6479},{maxDF:527,value:1.6478},{maxDF:545,value:1.6477},{maxDF:566,value:1.6476},{maxDF:587,value:1.6475},{maxDF:611,value:1.6474},{maxDF:636,value:1.6473},{maxDF:664,value:1.6472},{maxDF:694,value:1.6471},{maxDF:727,value:1.6470},{maxDF:764,value:1.6469},{maxDF:804,value:1.6468},{maxDF:849,value:1.6467},{maxDF:899,value:1.6466},{maxDF:955,value:1.6465},{maxDF:1019,value:1.6464},{maxDF:1092,value:1.6463},{maxDF:1176,value:1.6462},{maxDF:1274,value:1.6461},{maxDF:1390,value:1.6460},{maxDF:1530,value:1.6459},{maxDF:1700,value:1.6458},{maxDF:1914,value:1.6457},{maxDF:2189,value:1.6456},{maxDF:2555,value:1.6455},{maxDF:3070,value:1.6454},{maxDF:3845,value:1.6453},{maxDF:5142,value:1.6452},{maxDF:7760,value:1.6451},{maxDF:15812,value:1.6450},{maxDF:Infinity,value:1.6449}]},0.975:{probabilityToTValue:[12.7062,4.3027,3.1824,2.7764,2.5706,2.4469,2.3646,2.306,2.2622,2.2281,2.201,2.1788,2.1604,2.1448,2.1314,2.1199,2.1098,2.1009,2.093,2.086,2.0796,2.0739,2.0687,2.0639,2.0595,2.0555,2.0518,2.0484,2.0452,2.0423,2.0395,2.0369,2.0345,2.0322,2.0301,2.0281,2.0262,2.0244,2.0227,2.0211,2.0195,2.0181,2.0167,2.0154,2.0141,2.0129,2.0117,2.0106,2.0096,2.0086,2.0076,2.0066,2.0057,2.0049,2.004,2.0032,2.0025,2.0017,2.001,2.0003,1.9996,1.999,1.9983,1.9977,1.9971,1.9966,1.996,1.9955,1.9949,1.9944,1.9939,1.9935,1.993,1.9925,1.9921,1.9917,1.9913,1.9908,1.9905,1.9901,1.9897,1.9893,1.989,1.9886,1.9883,1.9879,1.9876,1.9873,1.987,1.9867,1.9864,1.9861,1.9858,1.9855,1.9853,1.985,1.9847,1.9845,1.9842,1.984,1.9837,1.9835,1.9833,1.983,1.9828,1.9826,1.9824,1.9822,1.982,1.9818,1.9816,1.9814,1.9812,1.981,1.9808,1.9806,1.9804,1.9803,1.9801,1.9799,1.9798,1.9796,1.9794,1.9793,1.9791,1.979,1.9788,1.9787,1.9785,1.9784,1.9782,1.9781,1.978,1.9778,1.9777,1.9776,1.9774,1.9773,1.9772,1.9771,1.9769,1.9768,1.9767,1.9766,1.9765,1.9763,1.9762,1.9761,1.976,1.9759,1.9758,1.9757,1.9756,1.9755,1.9754,1.9753,1.9752,1.9751,1.975,1.9749,1.9748,1.9747,1.9746,1.9745],tValuesSortedByProbability:[{maxDF:166,value:1.9744},{maxDF:167,value:1.9743},{maxDF:168,value:1.9742},{maxDF:169,value:1.9741},{maxDF:170,value:1.9740},{maxDF:172,value:1.9739},{maxDF:173,value:1.9738},{maxDF:174,value:1.9737},{maxDF:175,value:1.9736},{maxDF:177,value:1.9735},{maxDF:178,value:1.9734},{maxDF:179,value:1.9733},{maxDF:181,value:1.9732},{maxDF:182,value:1.9731},{maxDF:183,value:1.9730},{maxDF:185,value:1.9729},{maxDF:186,value:1.9728},{maxDF:188,value:1.9727},{maxDF:189,value:1.9726},{maxDF:191,value:1.9725},{maxDF:192,value:1.9724},{maxDF:194,value:1.9723},{maxDF:195,value:1.9722},{maxDF:197,value:1.9721},{maxDF:199,value:1.9720},{maxDF:200,value:1.9719},{maxDF:202,value:1.9718},{maxDF:204,value:1.9717},{maxDF:205,value:1.9716},{maxDF:207,value:1.9715},{maxDF:209,value:1.9714},{maxDF:211,value:1.9713},{maxDF:213,value:1.9712},{maxDF:215,value:1.9711},{maxDF:217,value:1.9710},{maxDF:219,value:1.9709},{maxDF:221,value:1.9708},{maxDF:223,value:1.9707},{maxDF:225,value:1.9706},{maxDF:227,value:1.9705},{maxDF:229,value:1.9704},{maxDF:231,value:1.9703},{maxDF:234,value:1.9702},{maxDF:236,value:1.9701},{maxDF:238,value:1.9700},{maxDF:241,value:1.9699},{maxDF:243,value:1.9698},{maxDF:246,value:1.9697},{maxDF:248,value:1.9696},{maxDF:251,value:1.9695},{maxDF:253,value:1.9694},{maxDF:256,value:1.9693},{maxDF:259,value:1.9692},{maxDF:262,value:1.9691},{maxDF:265,value:1.9690},{maxDF:268,value:1.9689},{maxDF:271,value:1.9688},{maxDF:274,value:1.9687},{maxDF:277,value:1.9686},{maxDF:280,value:1.9685},{maxDF:284,value:1.9684},{maxDF:287,value:1.9683},{maxDF:290,value:1.9682},{maxDF:294,value:1.9681},{maxDF:298,value:1.9680},{maxDF:302,value:1.9679},{maxDF:305,value:1.9678},{maxDF:309,value:1.9677},{maxDF:313,value:1.9676},{maxDF:318,value:1.9675},{maxDF:322,value:1.9674},{maxDF:326,value:1.9673},{maxDF:331,value:1.9672},{maxDF:335,value:1.9671},{maxDF:340,value:1.9670},{maxDF:345,value:1.9669},{maxDF:350,value:1.9668},{maxDF:355,value:1.9667},{maxDF:361,value:1.9666},{maxDF:366,value:1.9665},{maxDF:372,value:1.9664},{maxDF:378,value:1.9663},{maxDF:384,value:1.9662},{maxDF:390,value:1.9661},{maxDF:397,value:1.9660},{maxDF:404,value:1.9659},{maxDF:411,value:1.9658},{maxDF:418,value:1.9657},{maxDF:425,value:1.9656},{maxDF:433,value:1.9655},{maxDF:441,value:1.9654},{maxDF:449,value:1.9653},{maxDF:458,value:1.9652},{maxDF:467,value:1.9651},{maxDF:476,value:1.9650},{maxDF:486,value:1.9649},{maxDF:496,value:1.9648},{maxDF:507,value:1.9647},{maxDF:518,value:1.9646},{maxDF:530,value:1.9645},{maxDF:542,value:1.9644},{maxDF:554,value:1.9643},{maxDF:567,value:1.9642},{maxDF:581,value:1.9641},{maxDF:596,value:1.9640},{maxDF:611,value:1.9639},{maxDF:627,value:1.9638},{maxDF:644,value:1.9637},{maxDF:662,value:1.9636},{maxDF:681,value:1.9635},{maxDF:701,value:1.9634},{maxDF:723,value:1.9633},{maxDF:745,value:1.9632},{maxDF:769,value:1.9631},{maxDF:795,value:1.9630},{maxDF:823,value:1.9629},{maxDF:852,value:1.9628},{maxDF:884,value:1.9627},{maxDF:918,value:1.9626},{maxDF:955,value:1.9625},{maxDF:995,value:1.9624},{maxDF:1038,value:1.9623},{maxDF:1086,value:1.9622},{maxDF:1138,value:1.9621},{maxDF:1195,value:1.9620},{maxDF:1259,value:1.9619},{maxDF:1329,value:1.9618},{maxDF:1408,value:1.9617},{maxDF:1496,value:1.9616},{maxDF:1597,value:1.9615},{maxDF:1712,value:1.9614},{maxDF:1845,value:1.9613},{maxDF:2001,value:1.9612},{maxDF:2185,value:1.9611},{maxDF:2407,value:1.9610},{maxDF:2678,value:1.9609},{maxDF:3019,value:1.9608},{maxDF:3459,value:1.9607},{maxDF:4049,value:1.9606},{maxDF:4882,value:1.9605},{maxDF:6146,value:1.9604},{maxDF:8295,value:1.9603},{maxDF:12754,value:1.9602},{maxDF:27580,value:1.9601},{maxDF:Infinity,value:1.9600}]},0.99:{probabilityToTValue:[31.8205,6.9646,4.5407,3.7469,3.3649,3.1427,2.998,2.8965,2.8214,2.7638,2.7181,2.681,2.6503,2.6245,2.6025,2.5835,2.5669,2.5524,2.5395,2.528,2.5176,2.5083,2.4999,2.4922,2.4851,2.4786,2.4727,2.4671,2.462,2.4573,2.4528,2.4487,2.4448,2.4411,2.4377,2.4345,2.4314,2.4286,2.4258,2.4233,2.4208,2.4185,2.4163,2.4141,2.4121,2.4102,2.4083,2.4066,2.4049,2.4033,2.4017,2.4002,2.3988,2.3974,2.3961,2.3948,2.3936,2.3924,2.3912,2.3901,2.389,2.388,2.387,2.386,2.3851,2.3842,2.3833,2.3824,2.3816,2.3808,2.38,2.3793,2.3785,2.3778,2.3771,2.3764,2.3758,2.3751,2.3745,2.3739,2.3733,2.3727,2.3721,2.3716,2.371,2.3705,2.37,2.3695,2.369,2.3685,2.368,2.3676,2.3671,2.3667,2.3662,2.3658,2.3654,2.365,2.3646,2.3642,2.3638,2.3635,2.3631,2.3627,2.3624,2.362,2.3617,2.3614,2.361,2.3607,2.3604,2.3601,2.3598,2.3595,2.3592,2.3589,2.3586,2.3584,2.3581,2.3578,2.3576,2.3573,2.357,2.3568,2.3565,2.3563,2.3561,2.3558,2.3556,2.3554,2.3552,2.3549,2.3547,2.3545,2.3543,2.3541,2.3539,2.3537,2.3535,2.3533,2.3531,2.3529,2.3527,2.3525,2.3523,2.3522,2.352,2.3518,2.3516,2.3515,2.3513,2.3511,2.351,2.3508,2.3506,2.3505,2.3503,2.3502,2.35,2.3499,2.3497,2.3496,2.3494,2.3493,2.3492,2.349,2.3489,2.3487,2.3486,2.3485,2.3484,2.3482,2.3481,2.348,2.3478,2.3477,2.3476,2.3475,2.3474,2.3472,2.3471,2.347,2.3469,2.3468,2.3467,2.3466,2.3465,2.3463,2.3462,2.3461,2.346,2.3459,2.3458,2.3457,2.3456,2.3455,2.3454,2.3453,2.3452,2.3451],tValuesSortedByProbability:[{maxDF:201,value:2.3450},{maxDF:203,value:2.3449},{maxDF:204,value:2.3448},{maxDF:205,value:2.3447},{maxDF:206,value:2.3446},{maxDF:207,value:2.3445},{maxDF:208,value:2.3444},{maxDF:209,value:2.3443},{maxDF:211,value:2.3442},{maxDF:212,value:2.3441},{maxDF:213,value:2.3440},{maxDF:214,value:2.3439},{maxDF:215,value:2.3438},{maxDF:217,value:2.3437},{maxDF:218,value:2.3436},{maxDF:219,value:2.3435},{maxDF:220,value:2.3434},{maxDF:222,value:2.3433},{maxDF:223,value:2.3432},{maxDF:224,value:2.3431},{maxDF:226,value:2.3430},{maxDF:227,value:2.3429},{maxDF:228,value:2.3428},{maxDF:230,value:2.3427},{maxDF:231,value:2.3426},{maxDF:233,value:2.3425},{maxDF:234,value:2.3424},{maxDF:236,value:2.3423},{maxDF:237,value:2.3422},{maxDF:239,value:2.3421},{maxDF:240,value:2.3420},{maxDF:242,value:2.3419},{maxDF:243,value:2.3418},{maxDF:245,value:2.3417},{maxDF:246,value:2.3416},{maxDF:248,value:2.3415},{maxDF:250,value:2.3414},{maxDF:251,value:2.3413},{maxDF:253,value:2.3412},{maxDF:255,value:2.3411},{maxDF:256,value:2.3410},{maxDF:258,value:2.3409},{maxDF:260,value:2.3408},{maxDF:262,value:2.3407},{maxDF:264,value:2.3406},{maxDF:265,value:2.3405},{maxDF:267,value:2.3404},{maxDF:269,value:2.3403},{maxDF:271,value:2.3402},{maxDF:273,value:2.3401},{maxDF:275,value:2.3400},{maxDF:277,value:2.3399},{maxDF:279,value:2.3398},{maxDF:281,value:2.3397},{maxDF:283,value:2.3396},{maxDF:286,value:2.3395},{maxDF:288,value:2.3394},{maxDF:290,value:2.3393},{maxDF:292,value:2.3392},{maxDF:295,value:2.3391},{maxDF:297,value:2.3390},{maxDF:299,value:2.3389},{maxDF:302,value:2.3388},{maxDF:304,value:2.3387},{maxDF:307,value:2.3386},{maxDF:309,value:2.3385},{maxDF:312,value:2.3384},{maxDF:314,value:2.3383},{maxDF:317,value:2.3382},{maxDF:320,value:2.3381},{maxDF:322,value:2.3380},{maxDF:325,value:2.3379},{maxDF:328,value:2.3378},{maxDF:331,value:2.3377},{maxDF:334,value:2.3376},{maxDF:337,value:2.3375},{maxDF:340,value:2.3374},{maxDF:343,value:2.3373},{maxDF:346,value:2.3372},{maxDF:349,value:2.3371},{maxDF:353,value:2.3370},{maxDF:356,value:2.3369},{maxDF:360,value:2.3368},{maxDF:363,value:2.3367},{maxDF:367,value:2.3366},{maxDF:370,value:2.3365},{maxDF:374,value:2.3364},{maxDF:378,value:2.3363},{maxDF:381,value:2.3362},{maxDF:385,value:2.3361},{maxDF:389,value:2.3360},{maxDF:393,value:2.3359},{maxDF:398,value:2.3358},{maxDF:402,value:2.3357},{maxDF:406,value:2.3356},{maxDF:411,value:2.3355},{maxDF:415,value:2.3354},{maxDF:420,value:2.3353},{maxDF:425,value:2.3352},{maxDF:430,value:2.3351},{maxDF:435,value:2.3350},{maxDF:440,value:2.3349},{maxDF:445,value:2.3348},{maxDF:450,value:2.3347},{maxDF:456,value:2.3346},{maxDF:461,value:2.3345},{maxDF:467,value:2.3344},{maxDF:473,value:2.3343},{maxDF:479,value:2.3342},{maxDF:485,value:2.3341},{maxDF:492,value:2.3340},{maxDF:498,value:2.3339},{maxDF:505,value:2.3338},{maxDF:512,value:2.3337},{maxDF:519,value:2.3336},{maxDF:526,value:2.3335},{maxDF:534,value:2.3334},{maxDF:541,value:2.3333},{maxDF:549,value:2.3332},{maxDF:557,value:2.3331},{maxDF:566,value:2.3330},{maxDF:575,value:2.3329},{maxDF:584,value:2.3328},{maxDF:593,value:2.3327},{maxDF:602,value:2.3326},{maxDF:612,value:2.3325},{maxDF:622,value:2.3324},{maxDF:633,value:2.3323},{maxDF:644,value:2.3322},{maxDF:655,value:2.3321},{maxDF:667,value:2.3320},{maxDF:679,value:2.3319},{maxDF:691,value:2.3318},{maxDF:704,value:2.3317},{maxDF:718,value:2.3316},{maxDF:732,value:2.3315},{maxDF:747,value:2.3314},{maxDF:762,value:2.3313},{maxDF:778,value:2.3312},{maxDF:794,value:2.3311},{maxDF:811,value:2.3310},{maxDF:829,value:2.3309},{maxDF:848,value:2.3308},{maxDF:868,value:2.3307},{maxDF:888,value:2.3306},{maxDF:910,value:2.3305},{maxDF:933,value:2.3304},{maxDF:957,value:2.3303},{maxDF:982,value:2.3302},{maxDF:1008,value:2.3301},{maxDF:1036,value:2.3300},{maxDF:1066,value:2.3299},{maxDF:1097,value:2.3298},{maxDF:1130,value:2.3297},{maxDF:1166,value:2.3296},{maxDF:1203,value:2.3295},{maxDF:1243,value:2.3294},{maxDF:1286,value:2.3293},{maxDF:1332,value:2.3292},{maxDF:1381,value:2.3291},{maxDF:1434,value:2.3290},{maxDF:1491,value:2.3289},{maxDF:1553,value:2.3288},{maxDF:1621,value:2.3287},{maxDF:1694,value:2.3286},{maxDF:1775,value:2.3285},{maxDF:1864,value:2.3284},{maxDF:1962,value:2.3283},{maxDF:2070,value:2.3282},{maxDF:2192,value:2.3281},{maxDF:2329,value:2.3280},{maxDF:2484,value:2.3279},{maxDF:2661,value:2.3278},{maxDF:2865,value:2.3277},{maxDF:3103,value:2.3276},{maxDF:3385,value:2.3275},{maxDF:3722,value:2.3274},{maxDF:4135,value:2.3273},{maxDF:4650,value:2.3272},{maxDF:5312,value:2.3271},{maxDF:6194,value:2.3270},{maxDF:7428,value:2.3269},{maxDF:9274,value:2.3268},{maxDF:12344,value:2.3267},{maxDF:18450,value:2.3266},{maxDF:36515,value:2.3265},{maxDF:1754068,value:2.3264},{maxDF:Infinity,value:2.3263}]},};function oneSidedToTwoSidedProbability(probability){return 2*probability-1;}
function twoSidedToOneSidedProbability(probability){return(1-(1-probability)/2);}
function splitIntoSegmentsUntilGoodEnough(values,BirgeAndMassartC){if(values.length<2)
return[0,values.length];var matrix=new SampleVarianceUpperTriangularMatrix(values);var SchwarzCriterionBeta=Math.log1p(values.length-1)/values.length;BirgeAndMassartC=BirgeAndMassartC||2.5;var BirgeAndMassartPenalization=function(segmentCount){return segmentCount*(1+BirgeAndMassartC*Math.log1p(values.length/segmentCount-1));}
var segmentation;var minTotalCost=Infinity;var maxK=Math.min(50,values.length);for(var k=1;k<maxK;k++){var start=Date.now();var result=findOptimalSegmentation(values,matrix,k);var cost=result.cost/values.length;var penalty=SchwarzCriterionBeta*BirgeAndMassartPenalization(k);if(cost+penalty<minTotalCost){minTotalCost=cost+penalty;segmentation=result.segmentation;}else
maxK=Math.min(maxK,k+3);if(Statistics.debuggingSegmentation)
console.log('splitIntoSegmentsUntilGoodEnough',k,Date.now()-start,cost+penalty);}
return segmentation;}
function allocateCostUpperTriangularForSegmentation(values,segmentCount)
{var cost=new Array(values.length);for(var segmentEnd=0;segmentEnd<values.length;segmentEnd++)
cost[segmentEnd]=new Float32Array(segmentCount+1);return cost;}
function allocatePreviousNodeForSegmentation(values,segmentCount)
{var previousNode=new Array(values.length);for(var i=0;i<values.length;i++)
previousNode[i]=new Array(segmentCount+1);return previousNode;}
function findOptimalSegmentationInternal(cost,previousNode,values,costMatrix,segmentCount)
{cost[0]=[0];previousNode[0]=[-1];for(var segmentStart=0;segmentStart<values.length;segmentStart++){var costOfOptimalSegmentationThatEndAtCurrentStart=cost[segmentStart];for(var k=0;k<segmentCount;k++){var noSegmentationOfLenghtKEndsAtCurrentStart=previousNode[segmentStart][k]===undefined;if(noSegmentationOfLenghtKEndsAtCurrentStart)
continue;for(var segmentEnd=segmentStart+1;segmentEnd<values.length;segmentEnd++){var costOfOptimalSegmentationOfLengthK=costOfOptimalSegmentationThatEndAtCurrentStart[k];var costOfCurrentSegment=costMatrix.costBetween(segmentStart,segmentEnd);var totalCost=costOfOptimalSegmentationOfLengthK+costOfCurrentSegment;if(previousNode[segmentEnd][k+1]===undefined||totalCost<cost[segmentEnd][k+1]){cost[segmentEnd][k+1]=totalCost;previousNode[segmentEnd][k+1]=segmentStart;}}}}}
function findOptimalSegmentation(values,costMatrix,segmentCount){var cost=allocateCostUpperTriangularForSegmentation(values,segmentCount);var previousNode=allocatePreviousNodeForSegmentation(values,segmentCount);findOptimalSegmentationInternal(cost,previousNode,values,costMatrix,segmentCount);if(Statistics.debuggingSegmentation){console.log('findOptimalSegmentation with',segmentCount,'segments');for(var end=0;end<values.length;end++){for(var k=0;k<=segmentCount;k++){var start=previousNode[end][k];if(start===undefined)
continue;console.log(`C(segment=[${start}, ${end + 1}], segmentCount=${k})=${cost[end][k]}`);}}}
var segmentEnd=values.length-1;var segmentation=new Array(segmentCount+1);segmentation[segmentCount]=values.length;for(var k=segmentCount;k>0;k--){segmentEnd=previousNode[segmentEnd][k];segmentation[k-1]=segmentEnd;}
var costOfOptimalSegmentation=cost[values.length-1][segmentCount];if(Statistics.debuggingSegmentation)
console.log('Optimal segmentation:',segmentation,'with cost =',costOfOptimalSegmentation);return{segmentation:segmentation,cost:costOfOptimalSegmentation};}
function SampleVarianceUpperTriangularMatrix(values){var costMatrix=new Array(values.length-1);for(var i=0;i<values.length-1;i++){var remainingValueCount=values.length-i-1;costMatrix[i]=new Float32Array(remainingValueCount);var sum=values[i];var squareSum=sum*sum;costMatrix[i][0]=0;for(var j=i+1;j<values.length;j++){var currentValue=values[j];sum+=currentValue;squareSum+=currentValue*currentValue;var sampleSize=j-i+1;var stdev=Statistics.sampleStandardDeviation(sampleSize,sum,squareSum);costMatrix[i][j-i-1]=stdev>0?sampleSize*Math.log(stdev*stdev):0;}}
this.costMatrix=costMatrix;}
SampleVarianceUpperTriangularMatrix.prototype.costBetween=function(from,to){if(from>=this.costMatrix.length||from==to)
return 0;return this.costMatrix[from][to-from-1];}})();if(typeof module!='undefined')
module.exports=Statistics;class CommonRemoteAPI{postJSON(path,data,options)
{return this._asJSON(this.sendHttpRequest(path,'POST','application/json',JSON.stringify(data||{}),options));}
postJSONWithStatus(path,data,options)
{return this._checkStatus(this.postJSON(path,data,options));}
postFormData(path,data,options)
{const formData=new FormData();for(let key in data)
formData.append(key,data[key]);return this._asJSON(this.sendHttpRequestWithFormData(path,formData,options));}
postFormDataWithStatus(path,data,options)
{return this._checkStatus(this.postFormData(path,data,options));}
getJSON(path)
{return this._asJSON(this.sendHttpRequest(path,'GET',null,null));}
getJSONWithStatus(path)
{return this._checkStatus(this.getJSON(path));}
sendHttpRequest(path,method,contentType,content,options={})
{throw'NotImplemented';}
sendHttpRequestWithFormData(path,formData,options={})
{throw'NotImplemented';}
_asJSON(promise)
{return promise.then((result)=>{try{return JSON.parse(result.responseText);}catch(error){console.error(result.responseText);throw`{result.statusCode}: ${error}`;}});}
_checkStatus(promise)
{return promise.then(function(content){if(content['status']!='OK')
throw content['status'];return content;});}}
if(typeof module!='undefined')
module.exports.CommonRemoteAPI=CommonRemoteAPI;class CommonComponentBase{renderReplace(element,content){CommonComponentBase.renderReplace(element,content);}
static renderReplace(element,content)
{element.textContent='';if(content)
ComponentBase._addContentToElement(element,content);}
_recursivelyUpgradeUnknownElements(parent,findUpgrade,didConstructComponent=()=>{})
{let nextSibling;for(let child of parent.childNodes){const componentClass=findUpgrade(child);if(componentClass){const intance=this._upgradeUnknownElement(parent,child,componentClass);didConstructComponent(intance);}
if(child.childNodes)
this._recursivelyUpgradeUnknownElements(child,findUpgrade,didConstructComponent);}}
_upgradeUnknownElement(parent,unknownElement,componentClass)
{const instance=new componentClass;const newElement=instance.element();for(let i=0;i<unknownElement.attributes.length;i++){const attr=unknownElement.attributes[i];newElement.setAttribute(attr.name,attr.value);}
parent.replaceChild(newElement,unknownElement);for(const child of Array.from(unknownElement.childNodes))
newElement.appendChild(child);return instance;}
static _constructStylesheetFromTemplate(styleTemplate,didCreateRule=(selector,rule)=>selector)
{let stylesheet='';for(const selector in styleTemplate){const rules=styleTemplate[selector];let ruleText='';for(const property in rules){const value=rules[property];ruleText+=`    ${property}: ${value};\n`;}
const modifiedSelector=didCreateRule(selector,ruleText);stylesheet+=modifiedSelector+' {\n'+ruleText+'}\n\n';}
return stylesheet;}
static _constructNodeTreeFromTemplate(template,didCreateElement=(element)=>{})
{if(typeof(template)=='string')
return[CommonComponentBase._context.createTextNode(template)];console.assert(Array.isArray(template));if(typeof(template[0])=='string'){const tagName=template[0];let attributes={};let content=null;if(Array.isArray(template[1])){content=template[1];}else{attributes=template[1];content=template[2];}
const element=this.createElement(tagName,attributes);didCreateElement(element);const children=content&&content.length?this._constructNodeTreeFromTemplate(content,didCreateElement):[];for(const child of children)
element.appendChild(child);return[element];}else{let result=[];for(const item of template){if(typeof(item)=='string')
result.push(CommonComponentBase._context.createTextNode(item));else
result=result.concat(this._constructNodeTreeFromTemplate(item,didCreateElement));}
return result;}}
createElement(name,attributes,content){return CommonComponentBase.createElement(name,attributes,content);}
static createElement(name,attributes,content)
{const element=CommonComponentBase._context.createElement(name);if(!content&&(Array.isArray(attributes)||CommonComponentBase._isNode(attributes)||attributes instanceof CommonComponentBase._baseClass||typeof(attributes)!='object')){content=attributes;attributes={};}
if(attributes){for(const name in attributes){if(name.startsWith('on'))
element.addEventListener(name.substring(2),attributes[name]);else if(attributes[name]===true)
element.setAttribute(name,'');else if(attributes[name]!==false)
element.setAttribute(name,attributes[name]);}}
if(content)
CommonComponentBase._addContentToElement(element,content);return element;}
static _addContentToElement(element,content)
{if(Array.isArray(content)){for(var nestedChild of content)
this._addContentToElement(element,nestedChild);}else if(CommonComponentBase._isNode(content))
element.appendChild(content);else if(content instanceof CommonComponentBase._baseClass)
element.appendChild(content.element());else
element.appendChild(CommonComponentBase._context.createTextNode(content));}
createLink(content,titleOrCallback,callback,isExternal,tabIndex=null)
{return CommonComponentBase.createLink(content,titleOrCallback,callback,isExternal,tabIndex);}
static createLink(content,titleOrCallback,callback,isExternal,tabIndex=null)
{var title=titleOrCallback;if(callback===undefined){title=content;callback=titleOrCallback;}
var attributes={href:'#',title:title,};if(tabIndex)
attributes['tabindex']=tabIndex;if(typeof(callback)==='string')
attributes['href']=callback;else
attributes['onclick']=CommonComponentBase._baseClass.createEventHandler(callback);if(isExternal)
attributes['target']='_blank';return CommonComponentBase.createElement('a',attributes,content);}};CommonComponentBase._context=null;CommonComponentBase._isNode=null;CommonComponentBase._baseClass=null;if(typeof module!='undefined')
module.exports.CommonComponentBase=CommonComponentBase;class Instrumentation{static startMeasuringTime(domain,label)
{label=domain+':'+label;if(!Instrumentation._statistics){Instrumentation._statistics={};Instrumentation._currentMeasurement={};}
Instrumentation._currentMeasurement[label]=Date.now();}
static endMeasuringTime(domain,label)
{var time=Date.now()-Instrumentation._currentMeasurement[domain+':'+label];this.reportMeasurement(domain,label,'ms',time);}
static reportMeasurement(domain,label,unit,value)
{label=domain+':'+label;var statistics=Instrumentation._statistics;if(label in statistics){statistics[label].value+=value;statistics[label].count++;statistics[label].min=Math.min(statistics[label].min,value);statistics[label].max=Math.max(statistics[label].max,value);statistics[label].mean=statistics[label].value/statistics[label].count;}else
statistics[label]={value:value,unit:unit,count:1,mean:value,min:value,max:value};}
static dumpStatistics()
{if(!this._statistics)
return;var maxKeyLength=0;for(let key in this._statistics)
maxKeyLength=Math.max(key.length,maxKeyLength);for(let key of Object.keys(this._statistics).sort()){const item=this._statistics[key];const keySuffix=' '.repeat(maxKeyLength-key.length);let log=`${key}${keySuffix}: `;log+=` mean = ${item.mean.toFixed(2)} ${item.unit}`;if(item.unit=='ms')
log+=` total = ${item.value.toFixed(2)} ${item.unit}`;log+=` min = ${item.min.toFixed(2)} ${item.unit}`;log+=` max = ${item.max.toFixed(2)} ${item.unit}`;log+=` (${item.count} calls)`;console.log(log);}}}
if(typeof module!='undefined')
module.exports.Instrumentation=Instrumentation;class BrowserRemoteAPI extends CommonRemoteAPI{url(path)
{if(path.charAt(0)!='/')
path='/'+path;return`${location.protocol}//${location.host}${path}`;}
sendHttpRequest(path,method,contentType,content,options={})
{console.assert(!path.startsWith('http:')&&!path.startsWith('https:')&&!path.startsWith('file:'));return new Promise((resolve,reject)=>{Instrumentation.startMeasuringTime('Remote','sendHttpRequest');function onload(){Instrumentation.endMeasuringTime('Remote','sendHttpRequest');if(xhr.status!=200)
return reject(xhr.status);resolve({statusCode:xhr.status,responseText:xhr.responseText});};function onerror(){Instrumentation.endMeasuringTime('Remote','sendHttpRequest');reject(xhr.status);}
const xhr=new XMLHttpRequest;xhr.onload=onload;xhr.onabort=onerror;xhr.onerror=onerror;if(content&&options.uploadProgressCallback){xhr.upload.onprogress=(event)=>{options.uploadProgressCallback(event.lengthComputable?{total:event.total,loaded:event.loaded}:null);}}
xhr.open(method,path,true);if(contentType)
xhr.setRequestHeader('Content-Type',contentType);if(content)
xhr.send(content);else
xhr.send();});}
sendHttpRequestWithFormData(path,formData,options)
{return this.sendHttpRequest(path,'POST',null,formData,options);}}
const RemoteAPI=new BrowserRemoteAPI;class PrivilegedAPI{static sendRequest(path,data,options={useFormData:false})
{const clonedData={};for(let key in data)
clonedData[key]=data[key];const fullPath='/privileged-api/'+path;const post=options.useFormData?()=>RemoteAPI.postFormDataWithStatus(fullPath,clonedData,options):()=>RemoteAPI.postJSONWithStatus(fullPath,clonedData,options);return this.requestCSRFToken().then((token)=>{clonedData['token']=token;return post().catch((status)=>{if(status!='InvalidToken')
return Promise.reject(status);this._token=null;return this.requestCSRFToken().then((token)=>{clonedData['token']=token;return post();});});});}
static requestCSRFToken()
{const maxNetworkLatency=3*60*1000;if(this._token&&this._expiration>Date.now()+maxNetworkLatency)
return Promise.resolve(this._token);return RemoteAPI.postJSONWithStatus('/privileged-api/generate-csrf-token').then((result)=>{this._token=result['token'];this._expiration=new Date(result['expiration']);return this._token;});}}
PrivilegedAPI._token=null;PrivilegedAPI._expiration=null;if(typeof module!='undefined')
module.exports.PrivilegedAPI=PrivilegedAPI;class AsyncTask{constructor(method,args)
{this._method=method;this._args=args;}
execute()
{if(!(this._method in Statistics))
throw`${this._method} is not a valid method of Statistics`;AsyncTask._asyncMessageId++;var startTime=Date.now();var method=this._method;var args=this._args;return new Promise(function(resolve,reject){AsyncTaskWorker.waitForAvailableWorker(function(worker){worker.sendTask({id:AsyncTask._asyncMessageId,method:method,args:args}).then(function(data){var startLatency=data.workerStartTime-startTime;var totalTime=Date.now()-startTime;var callback=data.status=='resolve'?resolve:reject;callback({result:data.result,workerId:worker.id(),startLatency:startLatency,totalTime:totalTime,workerTime:data.workerTime});});});});}
static isAvailable()
{return typeof Worker!=='undefined';}}
AsyncTask._asyncMessageId=0;class AsyncTaskWorker{static waitForAvailableWorker(callback)
{var worker=this._makeWorkerEventuallyAvailable();if(worker)
callback(worker);this._queue.push(callback);}
static _makeWorkerEventuallyAvailable()
{var worker=this._findAvailableWorker();if(worker)
return worker;var canStartMoreWorker=this._workerSet.size<this._maxWorkerCount;if(!canStartMoreWorker)
return null;if(this._latestStartTime>Date.now()-50){setTimeout(function(){var worker=AsyncTaskWorker._findAvailableWorker();if(worker)
AsyncTaskWorker._queue.pop()(worker);},50);return null;}
return new AsyncTaskWorker;}
static _findAvailableWorker()
{for(var worker of this._workerSet){if(!worker._currentTaskId)
return worker;}
return null;}
constructor()
{this._webWorker=new Worker('async-task.js');this._webWorker.onmessage=this._didRecieveMessage.bind(this);this._id=AsyncTaskWorker._workerId;this._startTime=Date.now();this._currentTaskId=null;this._callback=null;AsyncTaskWorker._latestStartTime=this._startTime;AsyncTaskWorker._workerId++;AsyncTaskWorker._workerSet.add(this);}
id(){return this._id;}
sendTask(task)
{console.assert(!this._currentTaskId);console.assert(task.id);var self=this;this._currentTaskId=task.id;return new Promise(function(resolve){self._webWorker.postMessage(task);self._callback=resolve;});}
_didRecieveMessage(event)
{var callback=this._callback;console.assert(this._currentTaskId);this._currentTaskId=null;this._callback=null;if(AsyncTaskWorker._queue.length)
AsyncTaskWorker._queue.pop()(this);else{var self=this;setTimeout(function(){if(self._currentTaskId==null)
AsyncTaskWorker._workerSet.delete(self);},1000);}
callback(event.data);}
static workerDidRecieveMessage(event)
{var data=event.data;var id=data.id;var method=Statistics[data.method];var startTime=Date.now();try{var returnValue=method.apply(Statistics,data.args);postMessage({'id':id,'status':'resolve','result':returnValue,'workerStartTime':startTime,'workerTime':Date.now()-startTime});}catch(error){postMessage({'id':id,'status':'reject','result':error.toString(),'workerStartTime':startTime,'workerTime':Date.now()-startTime});throw error;}}}
AsyncTaskWorker._maxWorkerCount=typeof navigator!='undefined'&&'hardwareConcurrency'in navigator?Math.max(1,navigator.hardwareConcurrency-1):1;AsyncTaskWorker._workerSet=new Set;AsyncTaskWorker._queue=[];AsyncTaskWorker._workerId=1;AsyncTaskWorker._latestStartTime=0;if(typeof module=='undefined'&&typeof window=='undefined'&&typeof importScripts!='undefined'){ importScripts('/shared/statistics.js');onmessage=AsyncTaskWorker.workerDidRecieveMessage.bind(AsyncTaskWorker);}
if(typeof module!='undefined')
module.exports.AsyncTask=AsyncTask;class LazilyEvaluatedFunction{constructor(callback,...observedPropertiesList)
{console.assert(typeof(callback)=='function');this._callback=callback;this._observedPropertiesList=observedPropertiesList;this._cachedArguments=null;this._cachedResult=undefined;}
evaluate(...args)
{if(this._cachedArguments){const length=this._cachedArguments.length;if(args.length==length&&(!length||this._cachedArguments.every((cached,i)=>cached===args[i])))
return this._cachedResult;}
this._cachedArguments=args;this._cachedResult=this._callback.apply(null,args);return this._cachedResult;}}
if(typeof module!='undefined')
module.exports.LazilyEvaluatedFunction=LazilyEvaluatedFunction;class CommitSetRangeBisector{static async commitSetClosestToMiddleOfAllCommits(commitSetsToSplit,availableCommitSets)
{console.assert(commitSetsToSplit.length===2);const[firstCommitSet,secondCommitSet]=commitSetsToSplit;if(firstCommitSet.containsRootOrPatchOrOwnedCommit()||secondCommitSet.containsRootOrPatchOrOwnedCommit())
return null;if(!firstCommitSet.hasSameRepositories(secondCommitSet))
return null;const repositoriesWithCommitTime=new Set;const commitRangeByRepository=new Map;const indexForAllTimelessCommitsWithOrderByRepository=new Map;const allCommitsWithCommitTime=[];const repositoriesWithoutOrdering=[];await Promise.all(firstCommitSet.topLevelRepositories().map(async(repository)=>{const firstCommit=firstCommitSet.commitForRepository(repository);const secondCommit=secondCommitSet.commitForRepository(repository);if(!CommitLog.hasOrdering(firstCommit,secondCommit)){repositoriesWithoutOrdering.push(repository);commitRangeByRepository.set((repository),(commit)=>commit===firstCommit||commit===secondCommit);return;}
const[startCommit,endCommit]=CommitLog.orderTwoCommits(firstCommit,secondCommit);const commitsExcludingStartCommit=startCommit===endCommit?[]:await CommitLog.fetchBetweenRevisions(repository,startCommit.revision(),endCommit.revision());if(startCommit.hasCommitTime()){allCommitsWithCommitTime.push(startCommit,...commitsExcludingStartCommit);commitRangeByRepository.set(repository,(commit)=>commit.hasCommitTime()&&startCommit.time()<=commit.time()&&commit.time()<=endCommit.time());repositoriesWithCommitTime.add(repository);}else{const indexByCommit=new Map;indexByCommit.set(startCommit,0);commitsExcludingStartCommit.forEach((commit,index)=>indexByCommit.set(commit,index+1));indexForAllTimelessCommitsWithOrderByRepository.set(repository,indexByCommit);commitRangeByRepository.set(repository,(commit)=>commit.hasCommitOrder()&&startCommit.order()<=commit.order()&&commit.order()<=endCommit.order());}}));if(!repositoriesWithCommitTime.size&&!indexForAllTimelessCommitsWithOrderByRepository.size&&!repositoriesWithoutOrdering.size)
return null;const commitSetsInRange=this._findCommitSetsWithinRange(firstCommitSet,secondCommitSet,availableCommitSets,commitRangeByRepository);let sortedCommitSets=this._orderCommitSetsByTimeAndOrderThenDeduplicate(commitSetsInRange,repositoriesWithCommitTime,[...indexForAllTimelessCommitsWithOrderByRepository.keys()],repositoriesWithoutOrdering);if(!sortedCommitSets.length)
return null;let remainingCommitSets=this._closestCommitSetsToBisectingCommitByTime(sortedCommitSets,repositoriesWithCommitTime,allCommitsWithCommitTime);remainingCommitSets=this._findCommitSetsClosestToMiddleOfCommitsWithOrder(remainingCommitSets,indexForAllTimelessCommitsWithOrderByRepository);if(!remainingCommitSets.length)
return null;return remainingCommitSets[Math.floor(remainingCommitSets.length/2)];}
static _findCommitSetsWithinRange(firstCommitSetSpecifiedInRange,secondCommitSetSpecifiedInRange,availableCommitSets,commitRangeByRepository)
{return availableCommitSets.filter((commitSet)=>{if(!commitSet.hasSameRepositories(firstCommitSetSpecifiedInRange)||commitSet.equals(firstCommitSetSpecifiedInRange)||commitSet.equals(secondCommitSetSpecifiedInRange))
return false;for(const[repository,isCommitInRange]of commitRangeByRepository){const commit=commitSet.commitForRepository(repository);if(!isCommitInRange(commit))
return false;}
return true;});}
static _orderCommitSetsByTimeAndOrderThenDeduplicate(commitSets,repositoriesWithCommitTime,repositoriesWithCommitOrderOnly,repositoriesWithoutOrdering)
{const sortedCommitSets=commitSets.sort((firstCommitSet,secondCommitSet)=>{for(const repository of repositoriesWithCommitTime){const firstCommit=firstCommitSet.commitForRepository(repository);const secondCommit=secondCommitSet.commitForRepository(repository);const diff=firstCommit.time()-secondCommit.time();if(!diff)
continue;return diff;}
for(const repository of repositoriesWithCommitOrderOnly){const firstCommit=firstCommitSet.commitForRepository(repository);const secondCommit=secondCommitSet.commitForRepository(repository);const diff=firstCommit.order()-secondCommit.order();if(!diff)
continue;return diff;}
for(const repository of repositoriesWithoutOrdering){const firstCommit=firstCommitSet.commitForRepository(repository);const secondCommit=secondCommitSet.commitForRepository(repository);if(firstCommit===secondCommit)
continue;return firstCommit.revision()<secondCommit.revision()?-1:1;}
return 0;});return sortedCommitSets.filter((currentSet,i)=>!i||!currentSet.equals(sortedCommitSets[i-1]));}
static _closestCommitSetsToBisectingCommitByTime(sortedCommitSets,repositoriesWithCommitTime,allCommitsWithCommitTime)
{if(!repositoriesWithCommitTime.size)
return sortedCommitSets;const indexByCommitWithTime=new Map;allCommitsWithCommitTime.sort((firstCommit,secondCommit)=>firstCommit.time()-secondCommit.time()).forEach((commit,index)=>indexByCommitWithTime.set(commit,index));const commitToCommitSetMap=this._buildCommitToCommitSetMap(repositoriesWithCommitTime,sortedCommitSets);const closestCommit=this._findCommitClosestToMiddleIndex(indexByCommitWithTime,commitToCommitSetMap.keys());if(closestCommit)
return Array.from(commitToCommitSetMap.get(closestCommit));return[];}
static _findCommitSetsClosestToMiddleOfCommitsWithOrder(remainingCommitSets,indexForAllTimelessCommitsWithOrderByRepository)
{if(!indexForAllTimelessCommitsWithOrderByRepository.size)
return remainingCommitSets;const commitWithOrderToCommitSets=this._buildCommitToCommitSetMap(indexForAllTimelessCommitsWithOrderByRepository.keys(),remainingCommitSets);for(const[repository,indexByCommit]of indexForAllTimelessCommitsWithOrderByRepository){const commitsInRemainingSetsForCurrentRepository=remainingCommitSets.map((commitSet)=>commitSet.commitForRepository(repository));const closestCommit=this._findCommitClosestToMiddleIndex(indexByCommit,commitsInRemainingSetsForCurrentRepository);const commitSetsContainingClosestCommit=commitWithOrderToCommitSets.get(closestCommit);remainingCommitSets=remainingCommitSets.filter((commitSet)=>commitSetsContainingClosestCommit.has(commitSet));if(!remainingCommitSets.length)
return remainingCommitSets;}
return remainingCommitSets;}
static _buildCommitToCommitSetMap(repositories,commitSets)
{const commitToCommitSetMap=new Map;for(const repository of repositories){for(const commitSet of commitSets){const commit=commitSet.commitForRepository(repository);if(!commitToCommitSetMap.has(commit))
commitToCommitSetMap.set(commit,new Set);commitToCommitSetMap.get(commit).add(commitSet);}}
return commitToCommitSetMap;}
static _findCommitClosestToMiddleIndex(indexByCommit,commits)
{const desiredCommitIndex=indexByCommit.size/2;let minCommitDistance=indexByCommit.size;let closestCommit=null;for(const commit of commits){const index=indexByCommit.get(commit);const distanceForCommit=Math.abs(index-desiredCommitIndex);if(distanceForCommit<minCommitDistance){minCommitDistance=distanceForCommit;closestCommit=commit;}}
return closestCommit;}}
if(typeof module!='undefined')
module.exports.CommitSetRangeBisector=CommitSetRangeBisector;class TimeSeries{_data=[];values(){return this._data.map((point)=>point.value);}
length(){return this._data.length;}
append(item)
{console.assert(item.series===undefined);item.series=this;item.seriesIndex=this._data.length;this._data.push(item);}
extendToFuture()
{if(!this._data.length)
return;const lastPoint=this._data[this._data.length-1];this._data.push({series:this,seriesIndex:this._data.length,time:Date.now()+365*24*3600*1000,value:lastPoint.value,interval:lastPoint.interval,});}
valuesBetweenRange(startingIndex,endingIndex)
{startingIndex=Math.max(startingIndex,0);endingIndex=Math.min(endingIndex,this._data.length);const length=endingIndex-startingIndex;const values=new Array(length);for(let i=0;i<length;++i)
values[i]=this._data[startingIndex+i].value;return values;}
firstPoint(){return this._data.length?this._data[0]:null;}
lastPoint(){return this._data.length?this._data[this._data.length-1]:null;}
previousPoint(point)
{console.assert(point.series==this);if(!point.seriesIndex)
return null;return this._data[point.seriesIndex-1];}
nextPoint(point)
{console.assert(point.series==this);if(point.seriesIndex+1>=this._data.length)
return null;return this._data[point.seriesIndex+1];}
findPointByIndex(index)
{if(!this._data||index<0||index>=this._data.length)
return null;return this._data[index];}
findById(id){return this._data.find((point)=>point.id==id);}
findPointAfterTime(time){return this._data.find((point)=>point.time>=time);}
viewBetweenTime(startTime,endTime)
{const startPoint=this.findPointAfterTime(startTime);if(!startPoint)
return null;let endPoint=this.findPointAfterTime(endTime);if(!endPoint)
endPoint=this.lastPoint();else if(endPoint.time>endTime)
endPoint=this.previousPoint(endPoint);return this.viewBetweenPoints(startPoint,endPoint);}
viewBetweenPoints(firstPoint,lastPoint)
{console.assert(firstPoint.series==this);console.assert(lastPoint.series==this);return new TimeSeriesView(this,firstPoint.seriesIndex,lastPoint.seriesIndex+1);}};class TimeSeriesView{_timeSeries;_values=null;_length;_startingIndex;_afterEndingIndex;constructor(timeSeries,startingIndex,afterEndingIndex)
{console.assert(timeSeries instanceof TimeSeries);console.assert(startingIndex<=afterEndingIndex);console.assert(afterEndingIndex<=timeSeries._data.length);this._timeSeries=timeSeries;this._length=afterEndingIndex-startingIndex;this._startingIndex=startingIndex;this._afterEndingIndex=afterEndingIndex;}
get _data(){return this._timeSeries._data;}
length(){return this._length;}
firstPoint(){return this._length?this._data[this._startingIndex]:null;}
lastPoint(){return this._length?this._data[this._afterEndingIndex-1]:null;}
_findIndexForPoint(point)
{return point.seriesIndex;}
nextPoint(point)
{let index=this._findIndexForPoint(point);index++;if(index==this._afterEndingIndex)
return null;return this._data[index];}
previousPoint(point)
{const index=this._findIndexForPoint(point);if(index==this._startingIndex)
return null;return this._data[index-1];}
findPointByIndex(index)
{index+=this._startingIndex;if(index<0||index>=this._afterEndingIndex)
return null;return this._data[index];}
findById(id)
{for(let i=this._startingIndex;i<this._afterEndingIndex;++i){let point=this._data[i];if(point.id==id)
return point;}
return null;}
values()
{if(this._values==null){this._values=new Array(this._length);let i=0;for(let index=this._startingIndex;index<this._afterEndingIndex;++index){let point=this._data[index];this._values[i++]=point.value;}}
return this._values;}
filter(callback)
{const filteredData=[];let i=0;for(let index=this._startingIndex;index<this._afterEndingIndex;++index){let point=this._data[index];if(callback(point,i))
filteredData.push(point);i++;}
return new FilteredTimeSeriesView(this._timeSeries,0,filteredData.length,filteredData);}
viewTimeRange(startTime,endTime)
{const data=this._data;let startingIndex=null;let endingIndex=null;for(let i=this._startingIndex;i<this._afterEndingIndex;i++){if(startingIndex==null&&data[i].time>=startTime)
startingIndex=i;if(data[i].time<=endTime)
endingIndex=i;}
if(startingIndex==null||endingIndex==null)
return this._subRange(0,0);return this._subRange(startingIndex,endingIndex+1);}
_subRange(startingIndex,afterEndingIndex)
{return new TimeSeriesView(this._timeSeries,startingIndex,afterEndingIndex);}
firstPointInTimeRange(startTime,endTime)
{console.assert(startTime<=endTime);for(let index=this._startingIndex;index<this._afterEndingIndex;++index){let point=this._data[index];if(point.time>endTime)
return null;if(point.time>=startTime)
return point;}
return null;}
lastPointInTimeRange(startTime,endTime)
{console.assert(startTime<=endTime);for(let index=this._afterEndingIndex-1;index>=this._startingIndex;--index){let point=this._data[index];if(point.time<startTime)
return null;if(point.time<=endTime)
return point;}
return null;}}
class FilteredTimeSeriesView extends TimeSeriesView{_filteredData;_pointIndexMap;constructor(timeSeries,startingIndex,afterEndingIndex,filteredData)
{console.assert(afterEndingIndex<=filteredData.length);super(timeSeries,startingIndex,afterEndingIndex);this._filteredData=filteredData;}
get _data(){return this._filteredData;}
_subRange(startingIndex,afterEndingIndex)
{return new FilteredTimeSeriesView(this._timeSeries,startingIndex,afterEndingIndex,this._filteredData);}
_findIndexForPoint(point)
{if(!this._pointIndexMap)
this._buildPointIndexMap();return this._pointIndexMap.get(point);}
_buildPointIndexMap()
{this._pointIndexMap=new Map;const data=this._data;const length=data.length;for(let i=0;i<length;i++)
this._pointIndexMap.set(data[i],i);}}
if(typeof module!='undefined')
module.exports.TimeSeries=TimeSeries;class MeasurementAdaptor{constructor(formatMap)
{var nameMap={};formatMap.forEach(function(key,index){nameMap[key]=index;});this._idIndex=nameMap['id'];this._commitTimeIndex=nameMap['commitTime'];this._countIndex=nameMap['iterationCount'];this._meanIndex=nameMap['mean'];this._sumIndex=nameMap['sum'];this._squareSumIndex=nameMap['squareSum'];this._markedOutlierIndex=nameMap['markedOutlier'];this._revisionsIndex=nameMap['revisions'];this._buildIndex=nameMap['build'];this._buildTimeIndex=nameMap['buildTime'];this._buildTagIndex=nameMap['buildTag'];this._builderIndex=nameMap['builder'];this._metricIndex=nameMap['metric'];this._configTypeIndex=nameMap['configType'];}
extractId(row)
{return row[this._idIndex];}
isOutlier(row)
{return row[this._markedOutlierIndex];}
applyToAnalysisResults(row)
{var adaptedRow=this.applyTo(row);adaptedRow.metricId=row[this._metricIndex];adaptedRow.configType=row[this._configTypeIndex];return adaptedRow;}
applyTo(row)
{var id=row[this._idIndex];var mean=row[this._meanIndex];var sum=row[this._sumIndex];var squareSum=row[this._squareSumIndex];var buildId=row[this._buildIndex];var builderId=row[this._builderIndex];var cachedBuild=null;var cachedInterval=null;var self=this;return{id:id,markedOutlier:row[this._markedOutlierIndex],buildId:buildId,metricId:null,configType:null,commitSet:function(){return MeasurementCommitSet.ensureSingleton(id,row[self._revisionsIndex]);},build:function(){if(cachedBuild==null&&builderId)
cachedBuild=new Build(buildId,Builder.findById(builderId),row[self._buildTagIndex],row[self._buildTimeIndex]);return cachedBuild;},time:row[this._commitTimeIndex],value:mean,sum:sum,squareSum:squareSum,iterationCount:row[this._countIndex],get interval(){if(cachedInterval==null)
cachedInterval=MeasurementAdaptor.computeConfidenceInterval(row[self._countIndex],mean,sum,squareSum);return cachedInterval;}};}
static aggregateAnalysisResults(results)
{var totalSum=0;var totalSquareSum=0;var totalIterationCount=0;var means=[];for(var result of results){means.push(result.value);totalSum+=result.sum;totalSquareSum+=result.squareSum;totalIterationCount+=result.iterationCount;}
var mean=totalSum/totalIterationCount;var interval;try{interval=this.computeConfidenceInterval(totalIterationCount,mean,totalSum,totalSquareSum)}catch(error){interval=this.computeConfidenceInterval(results.length,mean,Statistics.sum(means),Statistics.squareSum(means));}
return{value:mean,interval:interval};}
static computeConfidenceInterval(iterationCount,mean,sum,squareSum)
{var delta=Statistics.confidenceIntervalDelta(0.95,iterationCount,sum,squareSum);return isNaN(delta)?null:[mean-delta,mean+delta];}}
if(typeof module!='undefined')
module.exports.MeasurementAdaptor=MeasurementAdaptor;class MeasurementCluster{constructor(response)
{this._response=response;this._adaptor=new MeasurementAdaptor(response['formatMap']);}
startTime(){return this._response['startTime'];}
endTime(){return this._response['endTime'];}
addToSeries(series,configType,includeOutliers,idMap)
{var rawMeasurements=this._response['configurations'][configType];if(!rawMeasurements)
return;var self=this;for(var row of rawMeasurements){var point=this._adaptor.applyTo(row);if(point.id in idMap||(!includeOutliers&&point.markedOutlier))
continue;series.append(point);idMap[point.id]=point.seriesIndex;point.cluster=this;}}}
if(typeof module!='undefined')
module.exports.MeasurementCluster=MeasurementCluster;class MeasurementSet{_platformId;_metricId;_lastModified;_sortedClusters=[];_primaryClusterEndTime=null;_clusterCount=null;_clusterStart=null;_clusterSize=null;_allFetches=new Map;_callbackMap=new Map;_primaryClusterPromise=null;_segmentationCache=new Map;constructor(platformId,metricId,lastModified)
{this._platformId=platformId;this._metricId=metricId;this._lastModified=+lastModified;}
platformId(){return this._platformId;}
metricId(){return this._metricId;}
static findSet(platformId,metricId,lastModified)
{if(!this._setMap)
this._setMap=new Map;const key=`${platformId}-${metricId}`;let measurementSet=this._setMap.get(key);if(!measurementSet){measurementSet=new MeasurementSet(platformId,metricId,lastModified);this._setMap.set(key,measurementSet);}
return measurementSet;}
findClusters(startTime,endTime)
{const clusterStart=this._clusterStart;const clusterSize=this._clusterSize;const clusters=[];let clusterEnd=clusterStart+Math.floor(Math.max(0,startTime-clusterStart)/clusterSize)*clusterSize;const lastClusterEndTime=this._primaryClusterEndTime;const firstClusterEndTime=lastClusterEndTime-clusterSize*(this._clusterCount-1);do{clusterEnd+=clusterSize;if(firstClusterEndTime<=clusterEnd&&clusterEnd<=this._primaryClusterEndTime)
clusters.push(clusterEnd);}while(clusterEnd<endTime);return clusters;}
async fetchBetween(startTime,endTime,callback,noCache)
{if(noCache){this._primaryClusterPromise=null;this._allFetches=new Map;}
if(!this._primaryClusterPromise)
this._primaryClusterPromise=this._fetchPrimaryCluster(noCache);if(callback)
this._primaryClusterPromise.catch(callback);await this._primaryClusterPromise;this._allFetches.set(this._primaryClusterEndTime,this._primaryClusterPromise);return Promise.all(this.findClusters(startTime,endTime).map((clusterEndTime)=>this._ensureClusterPromise(clusterEndTime,callback)));}
_ensureClusterPromise(clusterEndTime,callback)
{let callbackSet=this._callbackMap.get(clusterEndTime);if(!callbackSet){callbackSet=new Set;this._callbackMap.set(clusterEndTime,callbackSet);}
if(callback)
callbackSet.add(callback);let promise=this._allFetches.get(clusterEndTime);if(promise){if(callback)
promise.then(callback,callback);return promise;}
promise=this._fetchSecondaryCluster(clusterEndTime);for(const existingCallback of callbackSet)
promise.then(existingCallback,existingCallback);this._allFetches.set(clusterEndTime,promise);return promise;}
_urlForCache(clusterEndTime=null)
{const suffix=clusterEndTime?'-'+ +clusterEndTime:'';return`/data/measurement-set-${this._platformId}-${this._metricId}${suffix}.json`;}
async _fetchPrimaryCluster(noCache=false)
{let data=null;if(!noCache){try{data=await RemoteAPI.getJSONWithStatus(this._urlForCache());if(+data['lastModified']<this._lastModified)
data=null;}catch(error){if(error!=404)
throw error;}}
if(!data)
data=await RemoteAPI.getJSONWithStatus(`/api/measurement-set?platform=${this._platformId}&metric=${this._metricId}`);this._primaryClusterEndTime=data['endTime'];this._clusterCount=data['clusterCount'];this._clusterStart=data['clusterStart'];this._clusterSize=data['clusterSize'];this._addFetchedCluster(new MeasurementCluster(data));}
async _fetchSecondaryCluster(endTime)
{console.assert(endTime);console.assert(this._primaryClusterEndTime);const data=await RemoteAPI.getJSONWithStatus(this._urlForCache(endTime));this._addFetchedCluster(new MeasurementCluster(data));}
_addFetchedCluster(cluster)
{for(let clusterIndex=0;clusterIndex<this._sortedClusters.length;clusterIndex++){const startTime=this._sortedClusters[clusterIndex].startTime();if(cluster.startTime()<=startTime){this._sortedClusters.splice(clusterIndex,startTime==cluster.startTime()?1:0,cluster);return;}}
this._sortedClusters.push(cluster);}
hasFetchedRange(startTime,endTime)
{console.assert(startTime<endTime);let foundStart=false;let previousEndTime=null;endTime=Math.min(endTime,this._primaryClusterEndTime);for(const cluster of this._sortedClusters){const containsStart=cluster.startTime()<=startTime&&startTime<=cluster.endTime();const containsEnd=cluster.startTime()<=endTime&&endTime<=cluster.endTime();const preceedingClusterIsMissing=previousEndTime!==null&&previousEndTime!=cluster.startTime();if(containsStart&&containsEnd)
return true;if(containsStart)
foundStart=true;if(foundStart&&preceedingClusterIsMissing)
return false;if(containsEnd)
return foundStart; previousEndTime=cluster.endTime();}
return false;}
fetchedTimeSeries(configType,includeOutliers,extendToFuture)
{Instrumentation.startMeasuringTime('MeasurementSet','fetchedTimeSeries');const series=new TimeSeries();const idMap={};for(const cluster of this._sortedClusters)
cluster.addToSeries(series,configType,includeOutliers,idMap);if(extendToFuture)
series.extendToFuture();Instrumentation.endMeasuringTime('MeasurementSet','fetchedTimeSeries');return series;}
async fetchSegmentation(segmentationName,parameters,configType,includeOutliers,extendToFuture)
{let cacheMap=this._segmentationCache.get(configType);if(!cacheMap){cacheMap=new WeakMap;this._segmentationCache.set(configType,cacheMap);}
const timeSeries=new TimeSeries;const idMap={};const promises=[];for(const cluster of this._sortedClusters){const clusterStart=timeSeries.length();cluster.addToSeries(timeSeries,configType,includeOutliers,idMap);const clusterEnd=timeSeries.length();promises.push(this._cachedClusterSegmentation(segmentationName,parameters,cacheMap,cluster,timeSeries,clusterStart,clusterEnd,idMap));}
if(!timeSeries.length())
return Promise.resolve(null);const clusterSegmentations=await Promise.all(promises);const segmentationSeries=[];const addSegmentMergingIdenticalSegments=(startingPoint,endingPoint)=>{const value=Statistics.mean(timeSeries.valuesBetweenRange(startingPoint.seriesIndex,endingPoint.seriesIndex));if(!segmentationSeries.length||value!==segmentationSeries[segmentationSeries.length-1].value){segmentationSeries.push({value:value,time:startingPoint.time,seriesIndex:startingPoint.seriesIndex,interval:function(){return null;}});segmentationSeries.push({value:value,time:endingPoint.time,seriesIndex:endingPoint.seriesIndex,interval:function(){return null;}});}else
segmentationSeries[segmentationSeries.length-1].seriesIndex=endingPoint.seriesIndex;};let startingIndex=0;for(const segmentation of clusterSegmentations){for(const endingIndex of segmentation){addSegmentMergingIdenticalSegments(timeSeries.findPointByIndex(startingIndex),timeSeries.findPointByIndex(endingIndex));startingIndex=endingIndex;}}
if(extendToFuture)
timeSeries.extendToFuture();addSegmentMergingIdenticalSegments(timeSeries.findPointByIndex(startingIndex),timeSeries.lastPoint());return segmentationSeries;}
async _cachedClusterSegmentation(segmentationName,parameters,cacheMap,cluster,timeSeries,clusterStart,clusterEnd,idMap)
{const cache=cacheMap.get(cluster);if(cache&&this._validateSegmentationCache(cache,segmentationName,parameters)){const segmentationByIndex=new Array(cache.segmentation.length);for(let i=0;i<cache.segmentation.length;i++){const id=cache.segmentation[i];if(!(id in idMap))
return null;segmentationByIndex[i]=idMap[id];}
return Promise.resolve(segmentationByIndex);}
const clusterValues=timeSeries.valuesBetweenRange(clusterStart,clusterEnd);const segmentationInClusterIndex=await this._invokeSegmentationAlgorithm(segmentationName,parameters,clusterValues);const segmentation=segmentationInClusterIndex.slice(1,-1).map((index)=>clusterStart+index);cacheMap.set(cluster,{segmentationName,segmentationParameters:parameters.slice(),segmentation:segmentation.map((index)=>timeSeries.findPointByIndex(index).id)});return segmentation;}
_validateSegmentationCache(cache,segmentationName,parameters)
{if(cache.segmentationName!=segmentationName)
return false;if(!!cache.segmentationParameters!=!!parameters)
return false;if(parameters){if(parameters.length!=cache.segmentationParameters.length)
return false;for(var i=0;i<parameters.length;i++){if(parameters[i]!=cache.segmentationParameters[i])
return false;}}
return true;}
async _invokeSegmentationAlgorithm(segmentationName,parameters,timeSeriesValues)
{const args=[timeSeriesValues].concat(parameters||[]);const timeSeriesIsShortEnoughForSyncComputation=timeSeriesValues.length<100;if(timeSeriesIsShortEnoughForSyncComputation||!AsyncTask.isAvailable()){Instrumentation.startMeasuringTime('_invokeSegmentationAlgorithm','syncSegmentation');const segmentation=Statistics[segmentationName].apply(timeSeriesValues,args);Instrumentation.endMeasuringTime('_invokeSegmentationAlgorithm','syncSegmentation');return Promise.resolve(segmentation);}
const task=new AsyncTask(segmentationName,args);const response=await task.execute();Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm','workerStartLatency','ms',response.startLatency);Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm','workerTime','ms',response.workerTime);Instrumentation.reportMeasurement('_invokeSegmentationAlgorithm','totalTime','ms',response.totalTime);return response.result;}}
if(typeof module!='undefined')
module.exports.MeasurementSet=MeasurementSet;class AnalysisResults{constructor()
{this._metricToBuildMap={};this._metricIds=[];this._lazilyComputedTopLevelTests=new LazilyEvaluatedFunction(this._computedTopLevelTests.bind(this));}
findResult(buildId,metricId)
{const map=this._metricToBuildMap[metricId];if(!map)
return null;return map[buildId];}
topLevelTestsForTestGroup(testGroup)
{return this._lazilyComputedTopLevelTests.evaluate(testGroup,...this._metricIds);}
_computedTopLevelTests(testGroup,...metricIds)
{const metrics=metricIds.map((metricId)=>Metric.findById(metricId));const tests=new Set(metrics.map((metric)=>metric.test()));const topLevelMetrics=metrics.filter((metric)=>!tests.has(metric.test().parentTest()));const topLevelTests=new Set;for(const request of testGroup.buildRequests()){for(const metric of topLevelMetrics){if(this.findResult(request.buildId(),metric.id()))
topLevelTests.add(metric.test());}}
return topLevelTests;}
containsTest(test)
{console.assert(test instanceof Test);for(const metric of test.metrics()){if(metric.id()in this._metricToBuildMap)
return true;}
return false;}
_computeHighestTests(metricIds)
{const testsInResults=new Set(metricIds.map((metricId)=>Metric.findById(metricId).test()));return[...testsInResults].filter((test)=>!testsInResults.has(test.parentTest()));}
add(measurement)
{console.assert(measurement.configType=='current');const metricId=measurement.metricId;if(!(metricId in this._metricToBuildMap)){this._metricToBuildMap[metricId]={};this._metricIds=Object.keys(this._metricToBuildMap);}
const map=this._metricToBuildMap[metricId];console.assert(!map[measurement.buildId]);map[measurement.buildId]=measurement;}
commitSetForRequest(buildRequest)
{if(!this._metricIds.length)
return null;const result=this.findResult(buildRequest.buildId(),this._metricIds[0]);if(!result)
return null;return result.commitSet();}
viewForMetric(metric)
{console.assert(metric instanceof Metric);return new AnalysisResultsView(this,metric);}
static fetch(taskId)
{taskId=parseInt(taskId);return RemoteAPI.getJSONWithStatus(`/api/measurement-set?analysisTask=${taskId}`).then(function(response){Instrumentation.startMeasuringTime('AnalysisResults','fetch');const adaptor=new MeasurementAdaptor(response['formatMap']);const results=new AnalysisResults;for(const rawMeasurement of response['measurements'])
results.add(adaptor.applyToAnalysisResults(rawMeasurement));Instrumentation.endMeasuringTime('AnalysisResults','fetch');return results;});}}
class AnalysisResultsView{constructor(analysisResults,metric)
{console.assert(analysisResults instanceof AnalysisResults);console.assert(metric instanceof Metric);this._results=analysisResults;this._metric=metric;}
metric(){return this._metric;}
resultForRequest(buildRequest)
{return this._results.findResult(buildRequest.buildId(),this._metric.id());}}
if(typeof module!=='undefined'){module.exports.AnalysisResults=AnalysisResults;}
class DataModelObject{_id;constructor(id)
{this._id=id;this.ensureNamedStaticMap('id')[id]=this;}
id(){return this._id;}
static ensureSingleton(id,object)
{const singleton=this.findById(id);if(singleton){singleton.updateSingleton(object)
return singleton;}
return new(this)(id,object);}
updateSingleton(object){}
static clearStaticMap(){this[DataModelObject.StaticMapSymbol]=null;}
static namedStaticMap(name)
{const staticMap=this[DataModelObject.StaticMapSymbol];return staticMap?staticMap.get(name):null;}
static ensureNamedStaticMap(name)
{if(!this[DataModelObject.StaticMapSymbol])
this[DataModelObject.StaticMapSymbol]=new Map;const staticMap=this[DataModelObject.StaticMapSymbol];let namedMap=staticMap.get(name);if(!namedMap){namedMap={};staticMap.set(name,namedMap);}
return namedMap;}
namedStaticMap(name){return this.__proto__.constructor.namedStaticMap(name);}
ensureNamedStaticMap(name){return this.__proto__.constructor.ensureNamedStaticMap(name);}
static findById(id)
{const idMap=this.namedStaticMap('id');return idMap?idMap[id]:null;}
static listForStaticMap(name)
{const list=[];const idMap=this.namedStaticMap(name);if(idMap){for(const id in idMap)
list.push(idMap[id]);}
return list;}
static all(){return this.listForStaticMap('id');}
static async cachedFetch(path,params={},noCache=false)
{const query=[];if(params){for(let key in params)
query.push(key+'='+escape(params[key]));}
if(query.length)
path+='?'+query.join('&');if(noCache)
return RemoteAPI.getJSONWithStatus(path);const cacheMap=this.ensureNamedStaticMap(DataModelObject.CacheMapSymbol);let promise=cacheMap[path];if(!promise){promise=RemoteAPI.getJSONWithStatus(path);cacheMap[path]=promise;}
const content=await cacheMap[path];return JSON.parse(JSON.stringify(content));}}
DataModelObject.StaticMapSymbol=Symbol();DataModelObject.CacheMapSymbol=Symbol();class LabeledObject extends DataModelObject{_name;constructor(id,object)
{super(id);this._name=object.name;}
updateSingleton(object){this._name=object.name;}
static sortByName(list)
{return list.sort((a,b)=>{if(a.name()<b.name())
return-1;else if(a.name()>b.name())
return 1;return 0;});}
sortByName(list){return LabeledObject.sortByName(list);}
name(){return this._name;}
label(){return this.name();}}
if(typeof module!='undefined'){module.exports.DataModelObject=DataModelObject;module.exports.LabeledObject=LabeledObject;}
class CommitLog extends DataModelObject{constructor(id,rawData)
{console.assert(parseInt(id)==id);super(id);console.assert(id==rawData.id)
this._repository=rawData.repository;console.assert(this._repository instanceof Repository);this._rawData=rawData;this._ownedCommits=null;this._ownerCommit=null;this._ownedCommitByOwnedRepository=new Map;}
updateSingleton(rawData)
{super.updateSingleton(rawData);console.assert(+this._rawData['time']==+rawData['time']);console.assert(this._rawData['revision']==rawData['revision']);console.assert(this._rawData['revisionIdentifier']==rawData['revisionIdentifier']);if(rawData.authorName)
this._rawData.authorName=rawData.authorName;if(rawData.message)
this._rawData.message=rawData.message;if(rawData.ownsCommits)
this._rawData.ownsCommits=rawData.ownsCommits;if(rawData.order)
this._rawData.order=rawData.order;if(rawData.testability)
this._rawData.testability=rawData.testability;}
repository(){return this._repository;}
time(){return new Date(this._rawData['time']);}
hasCommitTime(){return this._rawData['time']>0&&this._rawData['time']!=null;}
testability(){return this._rawData['testability'];}
author(){return this._rawData['authorName'];}
revision(){return this._rawData['revision'];}
revisionIdentifier(){return this._rawData['revisionIdentifier'];}
message(){return this._rawData['message'];}
url(){return this._repository.urlForRevision(this.revisionIdentifier()||this.revision());}
ownsCommits(){return this._rawData['ownsCommits'];}
ownedCommits(){return this._ownedCommits;}
ownerCommit(){return this._ownerCommit;}
order(){return this._rawData['order'];}
hasCommitOrder(){return this._rawData['order']!=null;}
setOwnerCommits(ownerCommit){this._ownerCommit=ownerCommit;}
label(){return CommitLog._formatttedRevision(this.revision(),this.revisionIdentifier());}
static _repositoryType(revision)
{if(parseInt(revision)==revision)
return'svn';if(revision.length==40)
return'git';return null;}
static _formatttedRevision(revision,revisionIdentifier=null)
{const formattedRevision=(()=>{switch(this._repositoryType(revision)){case'svn':return'r'+revision; case'git':return revision.substring(0,12);}
return revision;})();if(revisionIdentifier)
return`${revisionIdentifier} (${formattedRevision})`;return formattedRevision;}
title(){return this._repository.name()+' at '+this.label();}
diff(previousCommit)
{if(this==previousCommit)
previousCommit=null;const repository=this._repository;if(!previousCommit)
return{repository:repository,label:this.label(),url:this.url()};const toRevision=this.revision();const fromRevision=previousCommit.revision();const identifierPattern=/(?<number>\d+)@(?<branch>[\w\.\-]+)/;const repositoryType=CommitLog._repositoryType(toRevision);const label=((fromMatch,toMatch)=>{const separator=repositoryType=='git'?'..':(repositoryType=='svn'?'-':' - ');const revisionRange=`${CommitLog._formatttedRevision(fromRevision)}${separator}${CommitLog._formatttedRevision(toRevision)}`;if(fromMatch&&toMatch){console.assert(fromMatch.groups.branch==toMatch.groups.branch);return`${fromMatch.groups.number}-${toMatch.groups.number}@${fromMatch.groups.branch} (${revisionRange})`;}
if(fromMatch||toMatch)
return`${previousCommit.label()} - ${this.label()}`;return revisionRange;})(identifierPattern.exec(previousCommit.revisionIdentifier()),identifierPattern.exec(this.revisionIdentifier()));const from=previousCommit.revisionIdentifier()||fromRevision;const to=this.revisionIdentifier()||toRevision;return{repository,label,url:repository.urlForRevisionRange(from,to)};}
static fetchLatestCommitForPlatform(repository,platform)
{console.assert(repository instanceof Repository);console.assert(platform instanceof Platform);return this.cachedFetch(`/api/commits/${repository.id()}/latest`,{platform:platform.id()}).then((data)=>{const commits=data['commits'];if(!commits||!commits.length)
return null;const rawData=commits[0];rawData.repository=repository;return CommitLog.ensureSingleton(rawData.id,rawData);});}
static hasOrdering(firstCommit,secondCommit)
{return(firstCommit.hasCommitTime()&&secondCommit.hasCommitTime())||(firstCommit.hasCommitOrder()&&secondCommit.hasCommitOrder());}
static orderTwoCommits(firstCommit,secondCommit)
{console.assert(CommitLog.hasOrdering(firstCommit,secondCommit));const firstCommitSmaller=firstCommit.hasCommitTime()&&secondCommit.hasCommitTime()?firstCommit.time()<secondCommit.time():firstCommit.order()<secondCommit.order();return firstCommitSmaller?[firstCommit,secondCommit]:[secondCommit,firstCommit];}
ownedCommitForOwnedRepository(ownedRepository){return this._ownedCommitByOwnedRepository.get(ownedRepository);}
fetchOwnedCommits()
{if(!this.repository().ownedRepositories())
return Promise.reject();if(!this.ownsCommits())
return Promise.reject();if(this._ownedCommits)
return Promise.resolve(this._ownedCommits);return CommitLog.cachedFetch(`../api/commits/${this.repository().id()}/owned-commits?owner-revision=${escape(this.revision())}`).then((data)=>{this._ownedCommits=CommitLog._constructFromRawData(data);this._ownedCommits.forEach((ownedCommit)=>{ownedCommit.setOwnerCommits(this);this._ownedCommitByOwnedRepository.set(ownedCommit.repository(),ownedCommit);});return this._ownedCommits;});}
_buildOwnedCommitMap()
{const ownedCommitMap=new Map;for(const commit of this._ownedCommits)
ownedCommitMap.set(commit.repository(),commit);return ownedCommitMap;}
static ownedCommitDifferenceForOwnerCommits(...commits)
{console.assert(commits.length>=2);const ownedCommitRepositories=new Set;const ownedCommitMapList=commits.map((commit)=>{console.assert(commit);console.assert(commit._ownedCommits);const ownedCommitMap=commit._buildOwnedCommitMap();for(const repository of ownedCommitMap.keys())
ownedCommitRepositories.add(repository);return ownedCommitMap;});const difference=new Map;ownedCommitRepositories.forEach((ownedCommitRepository)=>{const ownedCommits=ownedCommitMapList.map((ownedCommitMap)=>ownedCommitMap.get(ownedCommitRepository));const uniqueOwnedCommits=new Set(ownedCommits);if(uniqueOwnedCommits.size>1)
difference.set(ownedCommitRepository,ownedCommits);});return difference;}
static async fetchBetweenRevisions(repository,precedingRevision,lastRevision)
{const data=await this.cachedFetch(`/api/commits/${repository.id()}/`,{precedingRevision,lastRevision});return this._constructFromRawData(data);}
static async fetchForSingleRevision(repository,revision,prefixMatch=false)
{const commit=repository.commitForRevision(revision);if(commit)
return[commit];let params={};if(prefixMatch)
params['prefix-match']=true;const data=await this.cachedFetch(`/api/commits/${repository.id()}/${revision}`,params);return this._constructFromRawData(data);}
static _constructFromRawData(data)
{return data['commits'].map((rawData)=>{const repositoryId=rawData.repository;const repository=Repository.findById(repositoryId);rawData.repository=repository;const commit=CommitLog.ensureSingleton(rawData.id,rawData);repository.setCommitForRevision(commit.revision(),commit);return commit;});}}
if(typeof module!='undefined')
module.exports.CommitLog=CommitLog;class Platform extends LabeledObject{constructor(id,object)
{super(id,object);this._label=object.label;this._metrics=object.metrics;this._lastModifiedByMetric=object.lastModifiedByMetric;this._isHidden=object.hidden;this._containingTests=null;this.ensureNamedStaticMap('name')[object.name]=this;for(var metric of this._metrics)
metric.addPlatform(this);this._group=object.group;if(this._group)
this._group.addPlatform(this);}
static findByName(name)
{var map=this.namedStaticMap('name');return map?map[name]:null;}
label()
{return this._label||this.name();}
isInSameGroupAs(other)
{if(!this.group()&&!other.group())
return this==other;return this.group()==other.group();}
hasTest(test)
{if(!this._containingTests){this._containingTests={};for(var metric of this._metrics){for(var currentTest=metric.test();currentTest;currentTest=currentTest.parentTest()){if(currentTest.id()in this._containingTests)
break;this._containingTests[currentTest.id()]=true;}}}
return test.id()in this._containingTests;}
hasMetric(metric){return!!this.lastModified(metric);}
lastModified(metric)
{console.assert(metric instanceof Metric);return this._lastModifiedByMetric[metric.id()];}
isHidden()
{return this._isHidden;}
group(){return this._group;}}
if(typeof module!='undefined')
module.exports.Platform=Platform;class PlatformGroup extends LabeledObject{constructor(id,object)
{super(id,object);this.ensureNamedStaticMap('name')[object.name]=this;this._platforms=new Set;}
addPlatform(platform){this._platforms.add(platform);}
platforms(){return Array.from(this._platforms);}}
if(typeof module!='undefined')
module.exports.PlatformGroup=PlatformGroup;class Builder extends LabeledObject{constructor(id,object)
{super(id,object);this._buildUrlTemplate=object.buildUrl;}
urlForBuild(buildTag)
{if(!this._buildUrlTemplate)
return null;return this._buildUrlTemplate.replace(/\$builderName/g,this.name()).replace(/\$buildTag/g,buildTag);}}
class Build extends DataModelObject{constructor(id,builder,buildTag,buildTime)
{console.assert(builder instanceof Builder);super(id);this._builder=builder;this._buildTag=buildTag;this._buildTime=new Date(buildTime);}
builder(){return this._builder;}
buildTag(){return this._buildTag;}
buildTime(){return this._buildTime;}
label(){return`Build ${this._buildTag} on ${this._builder.label()}`;}
url(){return this._builder.urlForBuild(this._buildTag);}}
if(typeof module!='undefined'){module.exports.Builder=Builder;module.exports.Build=Build;}
class Test extends LabeledObject{constructor(id,object)
{super(id,object);this._url=object.url; this._parentId=object.parentId;this._childTests=[];this._metrics=[];this._hidden=object.hidden;this._computePathLazily=new LazilyEvaluatedFunction(this._computePath.bind(this));if(!this._parentId)
this.ensureNamedStaticMap('topLevelTests')[id]=this;else{var childMap=this.ensureNamedStaticMap('childTestMap');if(!childMap[this._parentId])
childMap[this._parentId]=[this];else
childMap[this._parentId].push(this);}}
static topLevelTests(){return this.sortByName(this.listForStaticMap('topLevelTests'));}
static findByPath(path)
{var matchingTest=null;var testList=this.topLevelTests();for(var part of path){matchingTest=null;for(var test of testList){if(part==test.name()){matchingTest=test;break;}}
if(!matchingTest)
return null;testList=matchingTest.childTests();}
return matchingTest;}
parentTest(){return Test.findById(this._parentId);}
path(){return this._computePathLazily.evaluate();}
_computePath()
{const path=[];let currentTest=this;while(currentTest){path.unshift(currentTest);currentTest=currentTest.parentTest();}
return path;}
fullName(){return this.path().map((test)=>test.label()).join(' \u220B ');}
relativeName(sharedPath)
{const path=this.path();const partialName=(index)=>path.slice(index).map((test)=>test.label()).join(' \u220B ');if(!sharedPath||!sharedPath.length)
return partialName(0);let i=0;for(;i<path.length&&i<sharedPath.length;i++){if(sharedPath[i]!=path[i])
return partialName(i);}
if(i<path.length)
return partialName(i);return null;}
onlyContainsSingleMetric(){return!this.childTests().length&&this._metrics.length==1;}
childTests()
{var childMap=this.namedStaticMap('childTestMap');return childMap&&this.id()in childMap?childMap[this.id()]:[];}
metrics(){return this._metrics;}
addChildTest(test){this._childTests.push(test);}
addMetric(metric){this._metrics.push(metric);}
isHidden(){return this._hidden;}}
if(typeof module!='undefined')
module.exports.Test=Test;class Metric extends LabeledObject{constructor(id,object)
{super(id,object);this._aggregatorName=object.aggregator;object.test.addMetric(this);this._test=object.test;this._platforms=[];const suffix=this.name().match('([A-z][a-z]+|FrameRate)$')[0];this._unit={'FrameRate':'fps','Runs':'/s','Time':'ms','Duration':'ms','Malloc':'B','Heap':'B','Allocations':'B','Size':'B','Score':'pt','Power':'W',}[suffix];}
aggregatorName(){return this._aggregatorName;}
test(){return this._test;}
platforms(){return this._platforms;}
addPlatform(platform)
{console.assert(platform instanceof Platform);this._platforms.push(platform);}
childMetrics()
{var metrics=[];for(var childTest of this._test.childTests()){for(var childMetric of childTest.metrics())
metrics.push(childMetric);}
return metrics;}
path(){return this._test.path().concat([this]);}
fullName(){return this._test.fullName()+' : '+this.label();}
relativeName(path)
{const relativeTestName=this._test.relativeName(path);if(relativeTestName==null)
return this.label();return relativeTestName+' : '+this.label();}
aggregatorLabel()
{switch(this._aggregatorName){case'Arithmetic':return'Arithmetic mean';case'Geometric':return'Geometric mean';case'Harmonic':return'Harmonic mean';case'Total':return'Total';}
return null;}
label()
{const aggregatorLabel=this.aggregatorLabel();return this.name()+(aggregatorLabel?` : ${aggregatorLabel}`:'');}
unit(){return this._unit;}
isSmallerBetter()
{const unit=this._unit;return unit!='fps'&&unit!='/s'&&unit!='pt';}
labelForDifference(beforeValue,afterValue,progressionTypeName,regressionTypeName)
{const diff=afterValue-beforeValue;const changeType=diff<0==this.isSmallerBetter()?progressionTypeName:regressionTypeName;const relativeChange=diff/beforeValue*100;const changeLabel=Math.abs(relativeChange).toFixed(2)+'% '+changeType;return{changeType,relativeChange,changeLabel};}
makeFormatter(sigFig,alwaysShowSign){return Metric.makeFormatter(this.unit(),sigFig,alwaysShowSign);}
static makeFormatter(unit,sigFig=2,alwaysShowSign=false)
{let isMiliseconds=false;if(unit=='ms'){isMiliseconds=true;unit='s';}
if(!unit)
return function(value){return value.toFixed(2)+' '+(unit||'');}
var divisor=unit=='B'?1024:1000;var suffix=['\u03BC','m','','K','M','G','T','P','E'];var threshold=sigFig>=3?divisor:(divisor/10);let formatter=function(value,maxAbsValue=0){var i;var sign=value>=0?(alwaysShowSign?'+':''):'-';value=Math.abs(value);let sigFigForValue=sigFig;
let adjustment=0;if(maxAbsValue&&value)
adjustment=Math.max(0,Math.floor(Math.log(maxAbsValue)/Math.log(10))-Math.floor(Math.log(value)/Math.log(10)));for(i=isMiliseconds?1:2;value&&value<1&&i>0;i--)
value*=divisor;for(;value>=threshold;i++)
value/=divisor;if(adjustment)
adjustment=Math.min(adjustment,Math.max(0,-Math.floor(Math.log(value)/Math.log(10))));return sign+value.toPrecision(sigFig-adjustment)+' '+suffix[i]+(unit||'');}
formatter.divisor=divisor;return formatter;};static formatTime(utcTime)
{const offsetInMinutes=(new Date(utcTime)).getTimezoneOffset();const timeInLocalTimeZone=new Date(utcTime-offsetInMinutes*60*1000);return timeInLocalTimeZone.toISOString().replace('T',' ').replace(/\.\d+Z$/,'');}}
if(typeof module!='undefined')
module.exports.Metric=Metric;class Repository extends LabeledObject{constructor(id,object)
{super(id,object);this._url=object.url;this._blameUrl=object.blameUrl;this._hasReportedCommits=object.hasReportedCommits;this._ownerId=object.owner;this._commitByRevision=new Map;if(!this._ownerId)
this.ensureNamedStaticMap('topLevelName')[this.name()]=this;else{const ownerships=this.ensureNamedStaticMap('repositoryOwnerships');if(!(this._ownerId in ownerships))
ownerships[this._ownerId]=[this];else
ownerships[this._ownerId].push(this);}}
static findTopLevelByName(name)
{const map=this.namedStaticMap('topLevelName');return map?map[name]:null;}
commitForRevision(revision){return this._commitByRevision.get(revision);}
setCommitForRevision(revision,commit){this._commitByRevision.set(revision,commit);}
hasUrlForRevision(){return!!this._url;}
urlForRevision(currentRevision)
{return(this._url||'').replace(/\$1/g,currentRevision);}
urlForRevisionRange(from,to)
{return(this._blameUrl||'').replace(/\$1/g,from).replace(/\$2/g,to);}
ownerId()
{return this._ownerId;}
ownedRepositories()
{const ownerships=this.namedStaticMap('repositoryOwnerships');return ownerships?ownerships[this.id()]:null;}
static sortByNamePreferringOnesWithURL(repositories)
{return repositories.sort(function(a,b){if(!!a._blameUrl==!!b._blameUrl){if(a.name()>b.name())
return 1;else if(a.name()<b.name())
return-1;return a.id()-b.id();}else if(b._blameUrl) 
return 1;return-1;});}}
if(typeof module!='undefined')
module.exports.Repository=Repository;class BugTracker extends LabeledObject{constructor(id,object)
{super(id,object);this._bugUrl=object.bugUrl;this._newBugUrl=object.newBugUrl;this._repositories=object.repositories;}
bugUrl(bugNumber){return this._bugUrl&&bugNumber?this._bugUrl.replace(/\$number/g,bugNumber):null;}
newBugUrl(){return this._newBugUrl;}
repositories(){return this._repositories;}}
if(typeof module!='undefined')
module.exports.BugTracker=BugTracker;class Bug extends DataModelObject{constructor(id,object)
{super(id,object);console.assert(object.bugTracker instanceof BugTracker);this._bugTracker=object.bugTracker;this._bugNumber=object.number;}
static ensureSingleton(object)
{console.assert(object.bugTracker instanceof BugTracker);var id=object.bugTracker.id()+'-'+object.number;return super.ensureSingleton(id,object);}
updateSingleton(object)
{super.updateSingleton(object);console.assert(this._bugTracker==object.bugTracker);console.assert(this._bugNumber==object.number);}
bugTracker(){return this._bugTracker;}
bugNumber(){return this._bugNumber;}
url(){return this._bugTracker.bugUrl(this._bugNumber);}
label(){return this.bugNumber();}
title(){return`${this._bugTracker.label()}: ${this.bugNumber()}`;}}
if(typeof module!='undefined')
module.exports.Bug=Bug;class AnalysisTask extends LabeledObject{constructor(id,object)
{super(id,object);this._author=object.author;this._createdAt=object.createdAt;console.assert(!object.platform||object.platform instanceof Platform);this._platform=object.platform;console.assert(!object.metric||object.metric instanceof Metric);this._metric=object.metric;this._startMeasurementId=object.startRun;this._startTime=object.startRunTime;this._endMeasurementId=object.endRun;this._endTime=object.endRunTime;this._category=object.category;this._changeType=object.result;this._needed=object.needed;this._bugs=object.bugs||[];this._causes=object.causes||[];this._fixes=object.fixes||[];this._buildRequestCount=+object.buildRequestCount;this._finishedBuildRequestCount=+object.finishedBuildRequestCount;}
static findByPlatformAndMetric(platformId,metricId)
{return this.all().filter((task)=>{const platform=task._platform;const metric=task._metric;return platform&&metric&&platform.id()==platformId&&metric.id()==metricId;});}
updateSingleton(object)
{super.updateSingleton(object);console.assert(this._author==object.author);console.assert(+this._createdAt==+object.createdAt);console.assert(this._platform==object.platform);console.assert(this._metric==object.metric);console.assert(this._startMeasurementId==object.startRun);console.assert(this._startTime==object.startRunTime);console.assert(this._endMeasurementId==object.endRun);console.assert(this._endTime==object.endRunTime);this._category=object.category;this._changeType=object.result;this._needed=object.needed;this._bugs=object.bugs||[];this._causes=object.causes||[];this._fixes=object.fixes||[];this._buildRequestCount=+object.buildRequestCount;this._finishedBuildRequestCount=+object.finishedBuildRequestCount;}
isCustom(){return!this._platform;}
hasResults(){return this._finishedBuildRequestCount;}
hasPendingRequests(){return this._finishedBuildRequestCount<this._buildRequestCount;}
requestLabel(){return`${this._finishedBuildRequestCount} of ${this._buildRequestCount}`;}
startMeasurementId(){return this._startMeasurementId;}
startTime(){return this._startTime;}
endMeasurementId(){return this._endMeasurementId;}
endTime(){return this._endTime;}
author(){return this._author||'';}
createdAt(){return this._createdAt;}
bugs(){return this._bugs;}
causes(){return this._causes;}
fixes(){return this._fixes;}
platform(){return this._platform;}
metric(){return this._metric;}
changeType(){return this._changeType;}
updateName(newName){return this._updateRemoteState({name:newName});}
updateChangeType(changeType){return this._updateRemoteState({result:changeType});}
_updateRemoteState(param)
{param.task=this.id();return PrivilegedAPI.sendRequest('update-analysis-task',param).then(function(data){return AnalysisTask.cachedFetch('/api/analysis-tasks',{id:param.task},true).then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));});}
associateBug(tracker,bugNumber)
{console.assert(tracker instanceof BugTracker);console.assert(typeof(bugNumber)=='number');var id=this.id();return PrivilegedAPI.sendRequest('associate-bug',{task:id,bugTracker:tracker.id(),number:bugNumber,}).then(function(data){return AnalysisTask.cachedFetch('/api/analysis-tasks',{id:id},true).then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));});}
dissociateBug(bug)
{console.assert(bug instanceof Bug);console.assert(this.bugs().includes(bug));var id=this.id();return PrivilegedAPI.sendRequest('associate-bug',{task:id,bugTracker:bug.bugTracker().id(),number:bug.bugNumber(),shouldDelete:true,}).then(function(data){return AnalysisTask.cachedFetch('/api/analysis-tasks',{id:id},true).then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));});}
associateCommit(kind,repository,revision)
{console.assert(kind=='cause'||kind=='fix');console.assert(repository instanceof Repository);if(revision.startsWith('r'))
revision=revision.substring(1);const id=this.id();return PrivilegedAPI.sendRequest('associate-commit',{task:id,repository:repository.id(),revision:revision,kind:kind,}).then((data)=>{return AnalysisTask.cachedFetch('/api/analysis-tasks',{id},true).then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));});}
dissociateCommit(commit)
{console.assert(commit instanceof CommitLog);var id=this.id();return PrivilegedAPI.sendRequest('associate-commit',{task:id,commit:commit.id(),}).then(function(data){return AnalysisTask.cachedFetch('/api/analysis-tasks',{id:id},true).then(AnalysisTask._constructAnalysisTasksFromRawData.bind(AnalysisTask));});}
category()
{var category='unconfirmed';if(this._changeType=='unchanged'||this._changeType=='inconclusive'||(this._changeType=='regression'&&this._fixes.length)||(this._changeType=='progression'&&(this._causes.length||this._fixes.length)))
category='closed';else if(this._causes.length||this._fixes.length||this._changeType=='regression'||this._changeType=='progression')
category='investigated';return category;}
async commitSetsFromTestGroupsAndMeasurementSet()
{const platform=this.platform();const metric=this.metric();if(!platform||!metric)
return[];const lastModified=platform.lastModified(metric);const measurementSet=MeasurementSet.findSet(platform.id(),metric.id(),lastModified);const fetchingMeasurementSetPromise=measurementSet.fetchBetween(this.startTime(),this.endTime());const commitSetsInSamePlatformGroupPromise=this._commitSetsInSamePlatformGroup();const allTestGroupsInTask=await TestGroup.fetchForTask(this.id());const allCommitSetsInTask=new Set;for(const group of allTestGroupsInTask)
group.requestedCommitSets().forEach((commitSet)=>allCommitSetsInTask.add(commitSet));await fetchingMeasurementSetPromise;const series=measurementSet.fetchedTimeSeries('current',false,false);const startPoint=series.findById(this.startMeasurementId());const endPoint=series.findById(this.endMeasurementId());const commitSetList=Array.from(series.viewBetweenPoints(startPoint,endPoint)).map((point)=>point.commitSet());const arrayOfCommitSetList=await Promise.all(commitSetsInSamePlatformGroupPromise);return[...commitSetList.concat(...arrayOfCommitSetList),...allCommitSetsInTask];}
_commitSetsInSamePlatformGroup()
{const metric=this.metric();if(!metric||!this.platform()||!this.platform().group())
return[];const otherPlatforms=this.platform().group().platforms().filter((platform)=>platform!=this.platform());return otherPlatforms.map(async(platform)=>{if(!platform.hasMetric(metric))
return[];const lastModified=platform.lastModified(metric);const measurementSet=MeasurementSet.findSet(platform.id(),metric.id(),lastModified);await measurementSet.fetchBetween(this.startTime(),this.endTime());const series=measurementSet.fetchedTimeSeries('current',false,false);const timeSeriesView=series.viewBetweenTime(this.startTime(),this.endTime());return timeSeriesView?Array.from(timeSeriesView).map((point)=>point.commitSet()):[];});}
static categories()
{return['unconfirmed','investigated','closed'];}
static fetchById(id,noCache)
{return this._fetchSubset({id:id},noCache).then((data)=>AnalysisTask.findById(id));}
static fetchByBuildRequestId(id)
{return this._fetchSubset({buildRequest:id}).then(function(tasks){return tasks[0];});}
static fetchByPlatformAndMetric(platformId,metricId,noCache)
{return this._fetchSubset({platform:platformId,metric:metricId},noCache).then(function(data){return AnalysisTask.findByPlatformAndMetric(platformId,metricId);});}
static fetchRelatedTasks(taskId)
{return this.fetchAll().then(function(){var task=AnalysisTask.findById(taskId);if(!task)
return undefined;var relatedTasks=new Set;for(var bug of task.bugs()){for(var otherTask of AnalysisTask.all()){if(otherTask.bugs().includes(bug))
relatedTasks.add(otherTask);}}
for(var otherTask of AnalysisTask.all()){if(task.isCustom())
continue;if(task.endTime()<otherTask.startTime()||otherTask.endTime()<task.startTime()||task.metric()!=otherTask.metric())
continue;relatedTasks.add(otherTask);}
return Array.from(relatedTasks);});}
static _fetchSubset(params,noCache)
{if(this._fetchAllPromise&&!noCache)
return this._fetchAllPromise;return this.cachedFetch('/api/analysis-tasks',params,noCache).then(this._constructAnalysisTasksFromRawData.bind(this));}
static fetchAll()
{if(!this._fetchAllPromise)
this._fetchAllPromise=RemoteAPI.getJSONWithStatus('/api/analysis-tasks').then(this._constructAnalysisTasksFromRawData.bind(this));return this._fetchAllPromise;}
static _constructAnalysisTasksFromRawData(data)
{Instrumentation.startMeasuringTime('AnalysisTask','construction');var taskToBug={};for(var rawData of data.bugs){rawData.bugTracker=BugTracker.findById(rawData.bugTracker);if(!rawData.bugTracker)
continue;var bug=Bug.ensureSingleton(rawData);if(!taskToBug[rawData.task])
taskToBug[rawData.task]=[];taskToBug[rawData.task].push(bug);}
for(var rawData of data.commits){rawData.repository=Repository.findById(rawData.repository);if(!rawData.repository)
continue;CommitLog.ensureSingleton(rawData.id,rawData);}
function resolveCommits(commits){return commits.map(function(id){return CommitLog.findById(id);}).filter(function(commit){return!!commit;});}
var results=[];for(var rawData of data.analysisTasks){rawData.platform=Platform.findById(rawData.platform);rawData.metric=Metric.findById(rawData.metric);rawData.bugs=taskToBug[rawData.id];rawData.causes=resolveCommits(rawData.causes);rawData.fixes=resolveCommits(rawData.fixes);results.push(AnalysisTask.ensureSingleton(rawData.id,rawData));}
Instrumentation.endMeasuringTime('AnalysisTask','construction');return results;}
static async create(name,startPoint,endPoint,testGroupName=null,repetitionCount=0,repetitionType='alternating',notifyOnCompletion=false)
{const parameters={name,startRun:startPoint.id,endRun:endPoint.id};if(testGroupName){console.assert(repetitionCount);parameters['revisionSets']=CommitSet.revisionSetsFromCommitSets([startPoint.commitSet(),endPoint.commitSet()]);parameters['repetitionCount']=repetitionCount;parameters['testGroupName']=testGroupName;parameters['needsNotification']=notifyOnCompletion;parameters['repetitionType']=repetitionType;}
const response=await PrivilegedAPI.sendRequest('create-analysis-task',parameters);return AnalysisTask.fetchById(response.taskId,true);}}
if(typeof module!='undefined')
module.exports.AnalysisTask=AnalysisTask;class TestGroup extends LabeledObject{constructor(id,object)
{super(id,object);this._taskId=object.task;this._authorName=object.author;this._createdAt=new Date(object.createdAt);this._isHidden=object.hidden;this._needsNotification=object.needsNotification;this._mayNeedMoreRequests=object.mayNeedMoreRequests;this._initialRepetitionCount=+object.initialRepetitionCount;this._repetitionType=object.repetitionType;this._buildRequests=[];this._orderBuildRequestsLazily=new LazilyEvaluatedFunction((...buildRequests)=>{return buildRequests.sort((a,b)=>a.order()-b.order());});this._repositories=null;this._computeRequestedCommitSetsLazily=new LazilyEvaluatedFunction(TestGroup._computeRequestedCommitSets);this._requestedCommitSets=null;this._commitSetToLabel=new Map;console.assert(!object.platform||object.platform instanceof Platform);this._platform=object.platform;}
updateSingleton(object)
{super.updateSingleton(object);console.assert(this._taskId==object.task);console.assert(+this._createdAt==+object.createdAt);console.assert(this._platform==object.platform);this._isHidden=object.hidden;this._needsNotification=object.needsNotification;this._notificationSentAt=object.notificationSentAt?new Date(object.notificationSentAt):null;this._mayNeedMoreRequests=object.mayNeedMoreRequests;this._initialRepetitionCount=+object.initialRepetitionCount;this._repetitionType=object.repetitionType;}
task(){return AnalysisTask.findById(this._taskId);}
createdAt(){return this._createdAt;}
isHidden(){return this._isHidden;}
buildRequests(){return this._buildRequests;}
needsNotification(){return this._needsNotification;}
mayNeedMoreRequests(){return this._mayNeedMoreRequests;}
initialRepetitionCount(){return this._initialRepetitionCount;}
repetitionType(){return this._repetitionType;}
notificationSentAt(){return this._notificationSentAt;}
author(){return this._authorName;}
addBuildRequest(request)
{this._buildRequests.push(request);this._requestedCommitSets=null;this._commitSetToLabel.clear();}
test()
{const request=this._lastRequest();return request?request.test():null;}
async fetchTask()
{if(this.task())
return this.task();return await AnalysisTask.fetchById(this._taskId);}
platform(){return this._platform;}
_lastRequest()
{const requests=this._orderedBuildRequests();return requests.length?requests[requests.length-1]:null;}
_orderedBuildRequests()
{return this._orderBuildRequestsLazily.evaluate(...this._buildRequests);}
repetitionCountForCommitSet(commitSet)
{if(!this._buildRequests.length)
return 0;let count=0;for(const request of this._buildRequests){if(request.isTest()&&request.commitSet()==commitSet)
count++;}
return count;}
hasRetries()
{return this.requestedCommitSets().some(commitSet=>this.repetitionCountForCommitSet(commitSet)>this.initialRepetitionCount());}
additionalRepetitionNeededToReachInitialRepetitionCount(commitSet)
{const potentiallySuccessfulCount=this.requestsForCommitSet(commitSet).filter((request)=>request.isTest()&&(request.hasCompleted()||!request.hasFinished())).length;return Math.max(0,this.initialRepetitionCount()-potentiallySuccessfulCount);}
successfulTestCount(commitSet)
{return this.requestsForCommitSet(commitSet).filter((request)=>request.isTest()&&request.hasCompleted()).length;}
isFirstTestRequest(buildRequest)
{return this._orderedBuildRequests().filter((request)=>request.isTest())[0]==buildRequest;}
precedingBuildRequest(buildRequest)
{const orderedBuildRequests=this._orderedBuildRequests();const buildRequestIndex=orderedBuildRequests.indexOf(buildRequest);console.assert(buildRequestIndex>=0);return orderedBuildRequests[buildRequestIndex-1];}
retryCountForCommitSet(commitSet)
{const retryCount=this.repetitionCountForCommitSet(commitSet)-this.initialRepetitionCount();console.assert(retryCount>=0);return retryCount;}
retryCountsAreSameForAllCommitSets()
{const retryCountForFirstCommitSet=this.retryCountForCommitSet(this.requestedCommitSets()[0]);return this.requestedCommitSets().every(commitSet=>this.retryCountForCommitSet(commitSet)==retryCountForFirstCommitSet);}
requestedCommitSets()
{return this._computeRequestedCommitSetsLazily.evaluate(...this._orderedBuildRequests());}
static _computeRequestedCommitSets(...orderedBuildRequests)
{const requestedCommitSets=[];for(const request of orderedBuildRequests){const set=request.commitSet();if(!requestedCommitSets.includes(set))
requestedCommitSets.push(set);}
return requestedCommitSets;}
requestsForCommitSet(commitSet)
{return this._orderedBuildRequests().filter((request)=>request.commitSet()==commitSet);}
labelForCommitSet(commitSet)
{const requestedSets=this.requestedCommitSets();const setIndex=requestedSets.indexOf(commitSet);if(setIndex<0)
return null;return String.fromCharCode('A'.charCodeAt(0)+setIndex);}
hasFinished()
{return this._buildRequests.every(function(request){return request.hasFinished();});}
hasStarted()
{return this._buildRequests.some(function(request){return request.hasStarted();});}
hasPending()
{return this._buildRequests.some(function(request){return request.isPending();});}
compareTestResults(metric,beforeMeasurements,afterMeasurements)
{console.assert(metric);const beforeValues=beforeMeasurements.map((measurment)=>measurment.value);const afterValues=afterMeasurements.map((measurement)=>measurement.value);const beforeMean=Statistics.sum(beforeValues)/beforeValues.length;const afterMean=Statistics.sum(afterValues)/afterValues.length;const result={changeType:null,status:'failed',label:'Failed',fullLabelForMean:'Failed',isStatisticallySignificantForMean:false,fullLabelForIndividual:'Failed',isStatisticallySignificantForIndividual:false,probabilityRangeForMean:[null,null],probabilityRangeForIndividual:[null,null]};const hasCompleted=this.hasFinished();if(!hasCompleted){if(this.hasStarted()){result.status='running';result.label='Running';result.fullLabelForMean='Running';result.fullLabelForIndividual='Running';}else{console.assert(result.changeType===null);result.status='pending';result.label='Pending';result.fullLabelForMean='Pending';result.fullLabelForIndividual='Pending';}}
if(beforeValues.length&&afterValues.length){const summary=metric.labelForDifference(beforeMean,afterMean,'better','worse');result.changeType=summary.changeType;result.label=summary.changeLabel;const isStatisticallySignificant=(probabilityRange)=>probabilityRange&&probabilityRange.range&&probabilityRange.range[0]&&probabilityRange.range[0]>=0.95;const constructSignificanceLabel=(probabilityRange)=>isStatisticallySignificant(probabilityRange)?`significant with ${(probabilityRange.range[0] * 100).toFixed()}% probability`:'insignificant';const probabilityRangeForMean=Statistics.probabilityRangeForWelchsT(beforeValues,afterValues);const significanceLabelForMean=constructSignificanceLabel(probabilityRangeForMean);result.fullLabelForMean=`${result.label} (${significanceLabelForMean})`;result.isStatisticallySignificantForMean=isStatisticallySignificant(probabilityRangeForMean);result.probabilityRangeForMean=probabilityRangeForMean.range;const adaptMeasurementToSamples=(measurement)=>({sum:measurement.sum,squareSum:measurement.squareSum,sampleSize:measurement.iterationCount});const probabilityRangeForIndividual=Statistics.probabilityRangeForWelchsTForMultipleSamples(beforeMeasurements.map(adaptMeasurementToSamples),afterMeasurements.map(adaptMeasurementToSamples));const significanceLabelForIndividual=constructSignificanceLabel(probabilityRangeForIndividual);result.fullLabelForIndividual=`${result.label} (${significanceLabelForIndividual})`;result.isStatisticallySignificantForIndividual=isStatisticallySignificant(probabilityRangeForIndividual.range[0]);result.probabilityRangeForIndividual=probabilityRangeForIndividual.range;if(hasCompleted)
result.status=result.isStatisticallySignificantForMean?result.changeType:'unchanged';}
return result;}
async _updateBuildRequest(content,endPoint='update-test-group')
{await PrivilegedAPI.sendRequest(endPoint,content);const data=await TestGroup.cachedFetch(`/api/test-groups/${this.id()}`,{},true);return TestGroup._createModelsFromFetchedTestGroups(data);}
updateName(newName)
{return this._updateBuildRequest({group:this.id(),name:newName,});}
updateHiddenFlag(hidden)
{return this._updateBuildRequest({group:this.id(),hidden:!!hidden,});}
async cancelPendingRequests()
{return this._updateBuildRequest({group:this.id(),cancel:true,});}
async didSendNotification()
{return await this._updateBuildRequest({group:this.id(),needsNotification:false,notificationSentAt:(new Date).toISOString()});}
async addMoreBuildRequests(addCount,commitSet=null)
{console.assert(!commitSet||this.repetitionType()=='sequential');console.assert(!commitSet||commitSet instanceof CommitSet);return await this._updateBuildRequest({group:this.id(),addCount,commitSet:commitSet?commitSet.id():null},'add-build-requests');}
async clearMayNeedMoreBuildRequests()
{return await this._updateBuildRequest({group:this.id(),mayNeedMoreRequests:false});}
static async createWithTask(taskName,platform,test,groupName,repetitionCount,repetitionType,commitSets,notifyOnCompletion)
{console.assert(commitSets.length==2);const revisionSets=CommitSet.revisionSetsFromCommitSets(commitSets);const data=await PrivilegedAPI.sendRequest('create-test-group',{taskName,name:groupName,platform:platform.id(),test:test.id(),repetitionCount,revisionSets,repetitionType,needsNotification:!!notifyOnCompletion});const task=await AnalysisTask.fetchById(data['taskId'],true);await this.fetchForTask(task.id());return task;}
static async createWithCustomConfiguration(task,platform,test,groupName,repetitionCount,repetitionType,commitSets,notifyOnCompletion)
{console.assert(commitSets.length==2);const revisionSets=CommitSet.revisionSetsFromCommitSets(commitSets);const data=await PrivilegedAPI.sendRequest('create-test-group',{task:task.id(),name:groupName,platform:platform.id(),test:test.id(),repetitionCount,repetitionType,revisionSets,needsNotification:!!notifyOnCompletion});await this.fetchById(data['testGroupId'],true);return this.findAllByTask(data['taskId']);}
static async createAndRefetchTestGroups(task,name,repetitionCount,repetitionType,commitSets,notifyOnCompletion)
{console.assert(commitSets.length==2);const revisionSets=CommitSet.revisionSetsFromCommitSets(commitSets);const data=await PrivilegedAPI.sendRequest('create-test-group',{task:task.id(),name,repetitionCount,repetitionType,revisionSets,needsNotification:!!notifyOnCompletion});await this.fetchById(data['testGroupId'],true);return this.findAllByTask(data['taskId']);}
async scheduleMoreRequestsOrClearFlag(maxRetryFactor)
{if(!this.mayNeedMoreRequests())
return 0;if(this.isHidden()){await this.clearMayNeedMoreBuildRequests();return 0;}
console.assert(TriggerableConfiguration.isValidRepetitionType(this.repetitionType()));const repetitionLimit=maxRetryFactor*this.initialRepetitionCount();const{retryCount,stopFutureRetries}=await(this.repetitionType()=='sequential'?this._createSequentialRetriesForTestGroup(repetitionLimit):this._createAlternatingRetriesForTestGroup(repetitionLimit));if(stopFutureRetries)
await this.clearMayNeedMoreBuildRequests();return retryCount;}
async _createAlternatingRetriesForTestGroup(repetitionLimit)
{const additionalRepetitionNeeded=this.requestedCommitSets().reduce((currentMax,commitSet)=>Math.max(this.additionalRepetitionNeededToReachInitialRepetitionCount(commitSet),currentMax),0);console.assert(additionalRepetitionNeeded<=this.initialRepetitionCount());const retryCount=this.requestedCommitSets().reduce((currentMin,commitSet)=>Math.min(Math.floor(repetitionLimit-this.repetitionCountForCommitSet(commitSet)),currentMin),additionalRepetitionNeeded);if(retryCount<=0)
return{retryCount:0,stopFutureRetries:true};const eachCommitSetHasCompletedAtLeastOneTest=this.requestedCommitSets().every(this.successfulTestCount.bind(this));if(!eachCommitSetHasCompletedAtLeastOneTest){const hasUnfinishedBuildRequest=this.requestedCommitSets().some((set)=>this.requestsForCommitSet(set).some((request)=>request.isTest()&&!request.hasFinished()));return{retryCount:0,stopFutureRetries:!hasUnfinishedBuildRequest};}
await this.addMoreBuildRequests(retryCount);return{retryCount,stopFutureRetries:false};}
async _createSequentialRetriesForTestGroup(repetitionLimit)
{const lastItem=(array)=>array[array.length-1];const commitSets=this.requestedCommitSets();console.assert(commitSets.every((currentSet,index)=>!index||lastItem(this.requestsForCommitSet(commitSets[index-1])).order()<this.requestsForCommitSet(currentSet)[0].order()));const allTestRequestsHaveFailedForSomeCommitSet=commitSets.some(commitSet=>this.requestsForCommitSet(commitSet).every(request=>!request.isTest()||request.hasFailed()));if(allTestRequestsHaveFailedForSomeCommitSet)
return{retryCount:0,stopFutureRetries:true};for(const commitSet of commitSets){if(this.successfulTestCount(commitSet)>=this.initialRepetitionCount())
continue;const additionalRepetition=this.additionalRepetitionNeededToReachInitialRepetitionCount(commitSet);const retryCount=Math.min(Math.floor(repetitionLimit-this.repetitionCountForCommitSet(commitSet)),additionalRepetition);if(retryCount<=0)
continue;await this.addMoreBuildRequests(retryCount,commitSet);return{retryCount,stopFutureRetries:false};}
return{retryCount:0,stopFutureRetries:true};}
static findAllByTask(taskId)
{return TestGroup.all().filter((testGroup)=>testGroup._taskId==taskId);}
static async fetchById(testGroupId,ignoreCache=false)
{const data=await this.cachedFetch(`/api/test-groups/${testGroupId}`,{},ignoreCache);this._createModelsFromFetchedTestGroups(data);return this.findById(testGroupId);}
static fetchForTask(taskId,ignoreCache=false)
{return this.cachedFetch('/api/test-groups',{task:taskId},ignoreCache).then(this._createModelsFromFetchedTestGroups.bind(this));}
static fetchAllWithNotificationReady()
{return this.cachedFetch('/api/test-groups/ready-for-notification',null,true).then(this._createModelsFromFetchedTestGroups.bind(this));}
static fetchAllThatMayNeedMoreRequests()
{return this.cachedFetch('/api/test-groups/need-more-requests',null,true).then(this._createModelsFromFetchedTestGroups.bind(this));}
static _createModelsFromFetchedTestGroups(data)
{var testGroups=data['testGroups'].map(function(row){row.platform=Platform.findById(row.platform);return TestGroup.ensureSingleton(row.id,row);});BuildRequest.constructBuildRequestsFromData(data);return testGroups;}}
if(typeof module!='undefined')
module.exports.TestGroup=TestGroup;class BuildRequest extends DataModelObject{constructor(id,object)
{super(id,object);this._triggerable=object.triggerable;console.assert(!object.repositoryGroup||object.repositoryGroup instanceof TriggerableRepositoryGroup);this._analysisTaskId=object.task;this._testGroupId=object.testGroupId;console.assert(!object.testGroup||object.testGroup instanceof TestGroup);this._testGroup=object.testGroup;if(this._testGroup)
this._testGroup.addBuildRequest(this);this._repositoryGroup=object.repositoryGroup;console.assert(object.platform instanceof Platform);this._platform=object.platform;console.assert(!object.test||object.test instanceof Test);this._test=object.test;this._order=+object.order;console.assert(object.commitSet instanceof CommitSet);this._commitSet=object.commitSet;this._status=object.status;this._statusUrl=object.url;this._buildId=object.build;this._createdAt=new Date(object.createdAt);this._result=null;this._statusDescription=object.statusDescription;}
updateSingleton(object)
{console.assert(+this._order<=+object.order);console.assert(this._commitSet==object.commitSet);const testGroup=object.testGroup;console.assert(!this._testGroup||this._testGroup==testGroup);if(!this._testGroup&&testGroup)
testGroup.addBuildRequest(this);this._testGroup=testGroup;this._status=object.status;this._statusUrl=object.url;this._buildId=object.build;this._statusDescription=object.statusDescription;this._order=+object.order;}
triggerable(){return this._triggerable;}
analysisTaskId(){return this._analysisTaskId;}
testGroupId(){return this._testGroupId;}
testGroup(){return this._testGroup;}
statusDescription(){return this._statusDescription;}
repositoryGroup(){return this._repositoryGroup;}
platform(){return this._platform;}
test(){return this._test;}
isBuild(){return this._order<0;}
isTest(){return this._order>=0;}
order(){return+this._order;}
commitSet(){return this._commitSet;}
status(){return this._status;}
hasFinished(){return this._status=='failed'||this._status=='completed'||this._status=='canceled';}
hasFailed(){return this._status=='failed';}
hasCompleted(){return this._status=='completed';}
hasStarted(){return this._status!='pending';}
isScheduled(){return this._status=='scheduled';}
isPending(){return this._status=='pending';}
statusLabel()
{switch(this._status){case'pending':return'Waiting';case'scheduled':return'Scheduled';case'running':return'Running';case'failed':return'Failed';case'completed':return'Completed';case'canceled':return'Canceled';}}
statusUrl(){return this._statusUrl;}
buildId(){return this._buildId;}
createdAt(){return this._createdAt;}
async findBuildRequestWithSameRoots()
{if(!this.isBuild())
return null;let scheduledBuildRequest=null;let runningBuildRequest=null;const allTestGroupsInTask=await TestGroup.fetchForTask(this.analysisTaskId(),true);const rawManifest=await Manifest.fetchRawResponse();const earliestRootCreatingTimeForReuse=rawManifest.maxRootReuseAgeInDays?Date.now()-rawManifest.maxRootReuseAgeInDays*24*3600*1000:0;for(const group of allTestGroupsInTask){if(group.id()==this.testGroupId())
continue;if(group.isHidden())
continue;for(const buildRequest of group.buildRequests()){if(!buildRequest.isBuild())
continue;if(!this.platform().isInSameGroupAs(buildRequest.platform()))
continue;if(!buildRequest.commitSet().equalsIgnoringRoot(this.commitSet()))
continue;if(!buildRequest.commitSet().areAllRootsAvailable(earliestRootCreatingTimeForReuse))
continue;if(buildRequest.hasCompleted())
return buildRequest;if(buildRequest.isScheduled()&&(!scheduledBuildRequest||buildRequest.createdAt()<scheduledBuildRequest.createdAt())){scheduledBuildRequest=buildRequest;}
if(buildRequest.status()=='running'&&(!runningBuildRequest||buildRequest.createdAt()<runningBuildRequest.createdAt())){runningBuildRequest=buildRequest;}}}
return runningBuildRequest||scheduledBuildRequest;}
static formatTimeInterval(intervalInMillionSeconds)
{let intervalInSeconds=intervalInMillionSeconds/1000;const units=[{unit:'week',length:7*24*3600},{unit:'day',length:24*3600},{unit:'hour',length:3600},{unit:'minute',length:60},];let indexOfFirstSmallEnoughUnit=units.length-1;for(let i=0;i<units.length;i++){if(intervalInSeconds>1.5*units[i].length){indexOfFirstSmallEnoughUnit=i;break;}}
let label='';let lastUnit=false;for(let i=indexOfFirstSmallEnoughUnit;!lastUnit;i++){lastUnit=i==indexOfFirstSmallEnoughUnit+1||i==units.length-1;const length=units[i].length;const valueForUnit=lastUnit?Math.round(intervalInSeconds/length):Math.floor(intervalInSeconds/length);const unit=units[i].unit+(valueForUnit==1?'':'s');if(label)
label+=' ';label+=`${valueForUnit} ${unit}`;intervalInSeconds=intervalInSeconds-valueForUnit*length;}
return label;}
waitingTime(referenceTime)
{return BuildRequest.formatTimeInterval(referenceTime-this.createdAt());}
static fetchForTriggerable(triggerable)
{return RemoteAPI.getJSONWithStatus('/api/build-requests/'+triggerable).then(function(data){return BuildRequest.constructBuildRequestsFromData(data);});}
static constructBuildRequestsFromData(data)
{for(let rawData of data['commits']){rawData.repository=Repository.findById(rawData.repository);CommitLog.ensureSingleton(rawData.id,rawData);}
for(let uploadedFile of data['uploadedFiles'])
UploadedFile.ensureSingleton(uploadedFile.id,uploadedFile);const commitSets=data['commitSets'].map((rawData)=>{for(const item of rawData.revisionItems){item.commit=CommitLog.findById(item.commit);item.patch=item.patch?UploadedFile.findById(item.patch):null;item.rootFile=item.rootFile?UploadedFile.findById(item.rootFile):null;item.commitOwner=item.commitOwner?CommitLog.findById(item.commitOwner):null;}
rawData.customRoots=rawData.customRoots.map((fileId)=>UploadedFile.findById(fileId));return CommitSet.ensureSingleton(rawData.id,rawData);});return data['buildRequests'].map(function(rawData){rawData.triggerable=Triggerable.findById(rawData.triggerable);rawData.repositoryGroup=TriggerableRepositoryGroup.findById(rawData.repositoryGroup);rawData.platform=Platform.findById(rawData.platform);rawData.test=Test.findById(rawData.test);rawData.testGroupId=rawData.testGroup;rawData.testGroup=TestGroup.findById(rawData.testGroup);rawData.commitSet=CommitSet.findById(rawData.commitSet);return BuildRequest.ensureSingleton(rawData.id,rawData);});}}
if(typeof module!='undefined')
module.exports.BuildRequest=BuildRequest;class CommitSet extends DataModelObject{constructor(id,object)
{super(id);this._repositories=[];this._repositoryToCommitMap=new Map;this._repositoryToPatchMap=new Map;this._repositoryToRootMap=new Map;this._repositoryToCommitOwnerMap=new Map;this._repositoryRequiresBuildMap=new Map;this._ownerRepositoryToOwnedRepositoriesMap=new Map;this._latestCommitTime=null;this._customRoots=[];this._allRootFiles=[];if(!object)
return;this._updateFromObject(object);}
updateSingleton(object)
{this._repositoryToCommitMap.clear();this._repositoryToPatchMap.clear();this._repositoryToRootMap.clear();this._repositoryToCommitOwnerMap.clear();this._repositoryRequiresBuildMap.clear();this._ownerRepositoryToOwnedRepositoriesMap.clear();this._repositories=[];this._updateFromObject(object);}
_updateFromObject(object)
{const rootFiles=new Set;for(const item of object.revisionItems){const commit=item.commit;console.assert(commit instanceof CommitLog);console.assert(!item.patch||item.patch instanceof UploadedFile);console.assert(!item.rootFile||item.rootFile instanceof UploadedFile);console.assert(!item.commitOwner||item.commitOwner instanceof CommitLog);const repository=commit.repository();this._repositoryToCommitMap.set(repository,commit);this._repositoryToPatchMap.set(repository,item.patch);if(item.commitOwner){this._repositoryToCommitOwnerMap.set(repository,item.commitOwner);const ownerRepository=item.commitOwner.repository();if(!this._ownerRepositoryToOwnedRepositoriesMap.get(ownerRepository))
this._ownerRepositoryToOwnedRepositoriesMap.set(ownerRepository,[repository]);else
this._ownerRepositoryToOwnedRepositoriesMap.get(ownerRepository).push(repository);}
this._repositoryRequiresBuildMap.set(repository,item.requiresBuild);this._repositoryToRootMap.set(repository,item.rootFile);if(item.rootFile)
rootFiles.add(item.rootFile);this._repositories.push(commit.repository());}
this._customRoots=object.customRoots;this._allRootFiles=Array.from(rootFiles).concat(object.customRoots);}
repositories(){return this._repositories;}
customRoots(){return this._customRoots;}
allRootFiles(){return this._allRootFiles;}
commitForRepository(repository){return this._repositoryToCommitMap.get(repository);}
ownerCommitForRepository(repository){return this._repositoryToCommitOwnerMap.get(repository);}
topLevelRepositories(){return Repository.sortByNamePreferringOnesWithURL(this._repositories.filter((repository)=>!this.ownerRevisionForRepository(repository)));}
ownedRepositoriesForOwnerRepository(repository){return this._ownerRepositoryToOwnedRepositoriesMap.get(repository);}
commitsWithTestability(){return this.commits().filter((commit)=>!!commit.testability());}
commits(){return Array.from(this._repositoryToCommitMap.values());}
areAllRootsAvailable(earliestCreationTime)
{return this.allRootFiles().every(rootFile=>this.customRoots().includes(rootFile)||(!rootFile.deletedAt()&&rootFile.createdAt()>=earliestCreationTime));}
revisionForRepository(repository)
{var commit=this._repositoryToCommitMap.get(repository);return commit?commit.revision():null;}
ownerRevisionForRepository(repository)
{const commit=this._repositoryToCommitOwnerMap.get(repository);return commit?commit.revision():null;}
patchForRepository(repository){return this._repositoryToPatchMap.get(repository);}
rootForRepository(repository){return this._repositoryToRootMap.get(repository);}
requiresBuildForRepository(repository){return this._repositoryRequiresBuildMap.get(repository)||false;}
latestCommitTime()
{if(this._latestCommitTime==null){var maxTime=0;for(const[repository,commit]of this._repositoryToCommitMap)
maxTime=Math.max(maxTime,+commit.time());this._latestCommitTime=maxTime;}
return this._latestCommitTime;}
equalsIgnoringRoot(other)
{return this._equalsOptionallyIgnoringRoot(other,true);}
equals(other)
{return this._equalsOptionallyIgnoringRoot(other,false);}
_equalsOptionallyIgnoringRoot(other,ignoringRoot)
{if(this._repositories.length!=other._repositories.length)
return false;for(const[repository,commit]of this._repositoryToCommitMap){if(commit!=other._repositoryToCommitMap.get(repository))
return false;if(this.patchForRepository(repository)!=other.patchForRepository(repository))
return false;if(this.rootForRepository(repository)!=other.rootForRepository(repository)&&!ignoringRoot)
return false;if(this.ownerCommitForRepository(repository)!=other.ownerCommitForRepository(repository))
return false;if(this.requiresBuildForRepository(repository)!=other.requiresBuildForRepository(repository))
return false;}
return CommitSet.areCustomRootsEqual(this._customRoots,other._customRoots);}
hasSameRepositories(commitSet)
{return commitSet.repositories().length===this._repositoryToCommitMap.size&&commitSet.repositories().every((repository)=>this._repositoryToCommitMap.has(repository));}
static areCustomRootsEqual(customRoots1,customRoots2)
{if(customRoots1.length!=customRoots2.length)
return false;const set2=new Set(customRoots2);for(let file of customRoots1){if(!set2.has(file))
return false;}
return true;}
static containsMultipleCommitsForRepository(commitSets,repository)
{console.assert(repository instanceof Repository);if(commitSets.length<2)
return false;const firstCommit=commitSets[0].commitForRepository(repository);for(let set of commitSets){const anotherCommit=set.commitForRepository(repository);if(!firstCommit!=!anotherCommit||(firstCommit&&firstCommit.revision()!=anotherCommit.revision()))
return true;}
return false;}
containsRootOrPatchOrOwnedCommit()
{if(this.allRootFiles().length)
return true;for(const repository of this.repositories()){if(this.ownerCommitForRepository(repository))
return true;if(this.ownedRepositoriesForOwnerRepository(repository))
return true;if(this.patchForRepository(repository))
return true;}
return false;}
static createNameWithoutCollision(name,existingNameSet)
{console.assert(existingNameSet instanceof Set);if(!existingNameSet.has(name))
return name;const nameWithNumberMatch=name.match(/(.+?)\s*\(\s*(\d+)\s*\)\s*$/);let number=1;if(nameWithNumberMatch){name=nameWithNumberMatch[1];number=parseInt(nameWithNumberMatch[2]);}
let newName;do{number++;newName=`${name} (${number})`;}while(existingNameSet.has(newName));return newName;}
static diff(firstCommitSet,secondCommitSet)
{console.assert(!firstCommitSet.equals(secondCommitSet));const allRepositories=new Set([...firstCommitSet.repositories(),...secondCommitSet.repositories()]);const sortedRepositories=Repository.sortByNamePreferringOnesWithURL([...allRepositories]);const nameParts=[];const missingCommit={label:()=>'none'};const missingPatch={filename:()=>'none'};const makeNameGenerator=()=>{const existingNameSet=new Set;return(name)=>{const newName=CommitSet.createNameWithoutCollision(name,existingNameSet);existingNameSet.add(newName);return newName;}};for(const repository of sortedRepositories){const firstCommit=firstCommitSet.commitForRepository(repository)||missingCommit;const secondCommit=secondCommitSet.commitForRepository(repository)||missingCommit;const firstPatch=firstCommitSet.patchForRepository(repository)||missingPatch;const secondPatch=secondCommitSet.patchForRepository(repository)||missingPatch;const nameGenerator=makeNameGenerator();if(firstCommit==secondCommit&&firstPatch==secondPatch)
continue;if(firstCommit!=secondCommit&&firstPatch==secondPatch)
nameParts.push(`${repository.name()}: ${secondCommit.diff(firstCommit).label}`);const nameForFirstPatch=nameGenerator(firstPatch.filename());const nameForSecondPath=nameGenerator(secondPatch.filename());if(firstCommit==secondCommit&&firstPatch!=secondPatch)
nameParts.push(`${repository.name()}: ${nameForFirstPatch} - ${nameForSecondPath}`);if(firstCommit!=secondCommit&&firstPatch!=secondPatch)
nameParts.push(`${repository.name()}: ${firstCommit.label()} with ${nameForFirstPatch} - ${secondCommit.label()} with ${nameForSecondPath}`);}
if(firstCommitSet.allRootFiles().length||secondCommitSet.allRootFiles().length){const firstRootFileSet=new Set(firstCommitSet.allRootFiles());const secondRootFileSet=new Set(secondCommitSet.allRootFiles());const uniqueInFirstCommitSet=firstCommitSet.allRootFiles().filter((rootFile)=>!secondRootFileSet.has(rootFile));const uniqueInSecondCommitSet=secondCommitSet.allRootFiles().filter((rootFile)=>!firstRootFileSet.has(rootFile));const nameGenerator=makeNameGenerator();const firstDescription=uniqueInFirstCommitSet.map((rootFile)=>nameGenerator(rootFile.filename())).join(', ');const secondDescription=uniqueInSecondCommitSet.map((rootFile)=>nameGenerator(rootFile.filename())).join(', ');nameParts.push(`Roots: ${firstDescription || 'none'} - ${secondDescription || 'none'}`);}
return nameParts.join(' ');}
static revisionSetsFromCommitSets(commitSets)
{return commitSets.map((commitSet)=>{console.assert(commitSet instanceof CustomCommitSet||commitSet instanceof CommitSet);const revisionSet={};for(let repository of commitSet.repositories()){const patchFile=commitSet.patchForRepository(repository);revisionSet[repository.id()]={revision:commitSet.revisionForRepository(repository),ownerRevision:commitSet.ownerRevisionForRepository(repository),patch:patchFile?patchFile.id():null,};}
const customRoots=commitSet.customRoots();if(customRoots&&customRoots.length)
revisionSet['customRoots']=customRoots.map((uploadedFile)=>uploadedFile.id());return revisionSet;});}}
class MeasurementCommitSet extends CommitSet{constructor(id,revisionList)
{super(id,null);for(const values of revisionList){const commitId=values[0];const repositoryId=values[1];const revision=values[2];const revisionIdentifier=values[3];const order=values[4];const time=values[5];const repository=Repository.findById(repositoryId);if(!repository)
continue;const commit=CommitLog.ensureSingleton(commitId,{id:commitId,repository,revision,revisionIdentifier,order,time});this._repositoryToCommitMap.set(repository,commit);this._repositories.push(repository);}}
namedStaticMap(name){return CommitSet.namedStaticMap(name);}
ensureNamedStaticMap(name){return CommitSet.ensureNamedStaticMap(name);}
static namedStaticMap(name){return CommitSet.namedStaticMap(name);}
static ensureNamedStaticMap(name){return CommitSet.ensureNamedStaticMap(name);}
static ensureSingleton(measurementId,revisionList)
{const commitSetId=measurementId+'-commitset';return CommitSet.findById(commitSetId)||(new MeasurementCommitSet(commitSetId,revisionList));}}
class CustomCommitSet{constructor()
{this._revisionListByRepository=new Map;this._customRoots=[];}
setRevisionForRepository(repository,revision,patch=null,ownerRevision=null)
{console.assert(repository instanceof Repository);console.assert(!patch||patch instanceof UploadedFile);this._revisionListByRepository.set(repository,{revision,patch,ownerRevision});}
equals(other)
{console.assert(other instanceof CustomCommitSet);if(this._revisionListByRepository.size!=other._revisionListByRepository.size)
return false;for(const[repository,thisRevision]of this._revisionListByRepository){const otherRevision=other._revisionListByRepository.get(repository);if(!thisRevision!=!otherRevision)
return false;if(thisRevision&&(thisRevision.revision!=otherRevision.revision||thisRevision.patch!=otherRevision.patch||thisRevision.ownerRevision!=otherRevision.ownerRevision))
return false;}
return CommitSet.areCustomRootsEqual(this._customRoots,other._customRoots);}
repositories(){return Array.from(this._revisionListByRepository.keys());}
topLevelRepositories(){return Repository.sortByNamePreferringOnesWithURL(this.repositories().filter((repository)=>!this.ownerRevisionForRepository(repository)));}
revisionForRepository(repository)
{const entry=this._revisionListByRepository.get(repository);if(!entry)
return null;return entry.revision;}
patchForRepository(repository)
{const entry=this._revisionListByRepository.get(repository);if(!entry)
return null;return entry.patch;}
ownerRevisionForRepository(repository)
{const entry=this._revisionListByRepository.get(repository);if(!entry)
return null;return entry.ownerRevision;}
customRoots(){return this._customRoots;}
addCustomRoot(uploadedFile)
{console.assert(uploadedFile instanceof UploadedFile);this._customRoots.push(uploadedFile);}}
class IntermediateCommitSet{constructor(commitSet)
{console.assert(commitSet instanceof CommitSet);this._commitByRepository=new Map;this._ownerToOwnedRepositories=new Map;this._fetchingPromiseByRepository=new Map;for(const repository of commitSet.repositories())
this.setCommitForRepository(repository,commitSet.commitForRepository(repository),commitSet.ownerCommitForRepository(repository));}
fetchCommitLogs()
{const fetchingPromises=[];for(const[repository,commit]of this._commitByRepository)
fetchingPromises.push(this._fetchCommitLogAndOwnedCommits(repository,commit.revision()));return Promise.all(fetchingPromises);}
commitsWithTestability(){return this.commits().filter((commit)=>!!commit.testability());}
commits(){return Array.from(this._commitByRepository.values());}
_fetchCommitLogAndOwnedCommits(repository,revision)
{return CommitLog.fetchForSingleRevision(repository,revision,true).then((commits)=>{console.assert(commits.length===1);const commit=commits[0];if(!commit.ownsCommits())
return commit;return commit.fetchOwnedCommits().then(()=>commit);});}
updateRevisionForOwnerRepository(repository,revision)
{const fetchingPromise=this._fetchCommitLogAndOwnedCommits(repository,revision);this._fetchingPromiseByRepository.set(repository,fetchingPromise);return fetchingPromise.then((commit)=>{const currentFetchingPromise=this._fetchingPromiseByRepository.get(repository);if(currentFetchingPromise!==fetchingPromise)
return;this._fetchingPromiseByRepository.set(repository,null);this.setCommitForRepository(repository,commit);});}
setCommitForRepository(repository,commit,ownerCommit=null)
{console.assert(repository instanceof Repository);console.assert(commit instanceof CommitLog);this._commitByRepository.set(repository,commit);if(!ownerCommit)
ownerCommit=commit.ownerCommit();if(ownerCommit){const ownerRepository=ownerCommit.repository();if(!this._ownerToOwnedRepositories.has(ownerRepository))
this._ownerToOwnedRepositories.set(ownerRepository,new Set);const repositorySet=this._ownerToOwnedRepositories.get(ownerRepository);repositorySet.add(repository);}}
removeCommitForRepository(repository)
{console.assert(repository instanceof Repository);this._fetchingPromiseByRepository.set(repository,null);const ownerCommit=this.ownerCommitForRepository(repository);if(ownerCommit){const repositorySet=this._ownerToOwnedRepositories.get(ownerCommit.repository());console.assert(repositorySet.has(repository));repositorySet.delete(repository);}else if(this._ownerToOwnedRepositories.has(repository)){const ownedRepositories=this._ownerToOwnedRepositories.get(repository);for(const ownedRepository of ownedRepositories)
this._commitByRepository.delete(ownedRepository);this._ownerToOwnedRepositories.delete(repository);}
this._commitByRepository.delete(repository);}
ownsCommitsForRepository(repository){return this.commitForRepository(repository).ownsCommits();}
repositories(){return Array.from(this._commitByRepository.keys());}
highestLevelRepositories(){return Repository.sortByNamePreferringOnesWithURL(this.repositories().filter((repository)=>!this.ownerCommitForRepository(repository)));}
commitForRepository(repository){return this._commitByRepository.get(repository);}
ownedRepositoriesForOwnerRepository(repository){return this._ownerToOwnedRepositories.get(repository);}
ownerCommitForRepository(repository)
{const commit=this._commitByRepository.get(repository);if(!commit)
return null;return commit.ownerCommit();}}
if(typeof module!='undefined'){module.exports.CommitSet=CommitSet;module.exports.MeasurementCommitSet=MeasurementCommitSet;module.exports.CustomCommitSet=CustomCommitSet;module.exports.IntermediateCommitSet=IntermediateCommitSet;}
class Triggerable extends LabeledObject{constructor(id,object)
{super(id,object);this._name=object.name;this._isDisabled=!!object.isDisabled;this._acceptedRepositories=object.acceptedRepositories;this._repositoryGroups=object.repositoryGroups;this._acceptedTests=new Set;this._triggerableConfigurationList=object.configurations.map(config=>{this._acceptedTests.add(config.test);return TriggerableConfiguration.createForTriggerable(this,config.test,config.platform,config.supportedRepetitionTypes);});}
name(){return this._name;}
isDisabled(){return this._isDisabled;}
repositoryGroups(){return this._repositoryGroups;}
acceptedTests(){return this._acceptedTests;}
static findByTestConfiguration(test,platform){return TriggerableConfiguration.findByTestAndPlatform(test,platform)?.triggerable;}
static triggerablePlatformsForTests(tests)
{console.assert(tests instanceof Array);if(!tests.length)
return[];return this.sortByName(Platform.all().filter((platform)=>{return tests.every((test)=>{const triggerable=Triggerable.findByTestConfiguration(test,platform);return triggerable&&!triggerable.isDisabled();});}));}}
class TriggerableConfiguration extends DataModelObject{#triggerable;#test;#platform;#supportedRepetitionTypes;constructor(id,object)
{super(id);this.#triggerable=object.triggerable;this.#test=object.test;this.#platform=object.platform;console.assert(object.supportedRepetitionTypes.every(TriggerableConfiguration.isValidRepetitionType.bind(TriggerableConfiguration)));this.#supportedRepetitionTypes=object.supportedRepetitionTypes;}
get triggerable(){return this.#triggerable;}
get supportedRepetitionTypes(){return[...this.#supportedRepetitionTypes];}
isSupportedRepetitionType(repetitionType){return this.#supportedRepetitionTypes.includes(repetitionType);}
static idForTestAndPlatform(test,platform){return`${test.id()}-${platform.id()}`;}
static createForTriggerable(triggerable,test,platform,supportedRepetitionTypes)
{const id=this.idForTestAndPlatform(test,platform);return TriggerableConfiguration.ensureSingleton(id,{test,platform,supportedRepetitionTypes,triggerable});}
static findByTestAndPlatform(test,platform)
{for(;test;test=test.parentTest()){const id=this.idForTestAndPlatform(test,platform);const triggerableConfiguration=TriggerableConfiguration.findById(id);if(triggerableConfiguration)
return triggerableConfiguration;}
return null;}
static isValidRepetitionType(repetitionType){return['alternating','sequential','paired-parallel'].includes(repetitionType);}}
class TriggerableRepositoryGroup extends LabeledObject{constructor(id,object)
{super(id,object);this._description=object.description;this._isHidden=!!object.hidden;this._acceptsCustomRoots=!!object.acceptsCustomRoots;this._repositories=Repository.sortByNamePreferringOnesWithURL(object.repositories.map((item)=>item.repository));this._patchAcceptingSet=new Set(object.repositories.filter((item)=>item.acceptsPatch).map((item)=>item.repository));}
accepts(commitSet)
{const commitSetRepositories=commitSet.topLevelRepositories();if(this._repositories.length!=commitSetRepositories.length)
return false;for(let i=0;i<this._repositories.length;i++){const currentRepository=this._repositories[i];if(currentRepository!=commitSetRepositories[i])
return false;if(commitSet.patchForRepository(currentRepository)&&!this._patchAcceptingSet.has(currentRepository))
return false;}
if(commitSet.customRoots().length&&!this._acceptsCustomRoots)
return false;return true;}
acceptsPatchForRepository(repository)
{return this._patchAcceptingSet.has(repository);}
description(){return this._description||this.name();}
isHidden(){return this._isHidden;}
acceptsCustomRoots(){return this._acceptsCustomRoots;}
repositories(){return this._repositories;}
static sortByNamePreferringSmallerRepositories(groupList)
{return groupList.sort((a,b)=>{if(a.repositories().length<b.repositories().length)
return-1;else if(a.repositories().length>b.repositories().length)
return 1;if(a.name()<b.name())
return-1;else if(a.name()>b.name())
return 1;return 0;});}}
if(typeof module!='undefined'){module.exports.Triggerable=Triggerable;module.exports.TriggerableConfiguration=TriggerableConfiguration;module.exports.TriggerableRepositoryGroup=TriggerableRepositoryGroup;}
class UploadedFile extends DataModelObject{constructor(id,object)
{super(id,object);this._createdAt=new Date(object.createdAt);this._deletedAt=object.deletedAt?new Date(object.deletedAt):null;this._filename=object.filename;this._extension=object.extension;this._author=object.author;this._size=object.size;this._sha256=object.sha256;this.ensureNamedStaticMap('sha256')[object.sha256]=this;}
createdAt(){return this._createdAt;}
deletedAt(){return this._deletedAt;}
filename(){return this._filename;}
author(){return this._author;}
size(){return this._size;}
label(){return this.filename();}
url(){return RemoteAPI.url(`/api/uploaded-file/${this.id()}${this._extension}`);}
static uploadFile(file,uploadProgressCallback=null)
{if(file.size>UploadedFile.fileUploadSizeLimit)
return Promise.reject('FileSizeLimitExceeded');return PrivilegedAPI.sendRequest('upload-file',{'newFile':file},{useFormData:true,uploadProgressCallback}).then((rawData)=>{return UploadedFile.ensureSingleton(rawData['uploadedFile'].id,rawData['uploadedFile']);});}
static fetchUploadedFileWithIdenticalHash(file)
{if(file.size>UploadedFile.fileUploadSizeLimit)
return Promise.reject('FileSizeLimitExceeded');return new Promise((resolve,reject)=>{const reader=new FileReader();reader.onload=()=>resolve(reader.result);reader.onerror=()=>reject();reader.readAsArrayBuffer(file);}).then((content)=>{return this._computeSHA256Hash(content);}).then((sha256)=>{const map=this.namedStaticMap('sha256');if(map&&sha256 in map)
return map[sha256];return RemoteAPI.getJSON(`../api/uploaded-file?sha256=${sha256}`).then((rawData)=>{if(rawData['status']=='NotFound'||!rawData['uploadedFile'])
return null;if(rawData['status']!='OK')
return Promise.reject(rawData['status']);return UploadedFile.ensureSingleton(rawData['uploadedFile'].id,rawData['uploadedFile']);});});}
static _computeSHA256Hash(content)
{return(crypto.subtle||crypto.webkitSubtle).digest('SHA-256',content).then((digest)=>{return Array.from(new Uint8Array(digest)).map((byte)=>{if(byte<0x10)
return'0'+byte.toString(16);return byte.toString(16);}).join('');});}}
UploadedFile.fileUploadSizeLimit=0;if(typeof module!='undefined')
module.exports.UploadedFile=UploadedFile;class Manifest{static reset()
{AnalysisTask._fetchAllPromise=null;AnalysisTask.clearStaticMap();BuildRequest.clearStaticMap();CommitLog.clearStaticMap();Metric.clearStaticMap();Platform.clearStaticMap();PlatformGroup.clearStaticMap();Repository.clearStaticMap();CommitSet.clearStaticMap();Test.clearStaticMap();TestGroup.clearStaticMap();Triggerable.clearStaticMap();TriggerableConfiguration.clearStaticMap();TriggerableRepositoryGroup.clearStaticMap();UploadedFile.clearStaticMap();BugTracker.clearStaticMap();Bug.clearStaticMap();}
static async fetch()
{this.reset();const rawManifest=await this.fetchRawResponse();return this._didFetchManifest(rawManifest);}
static async fetchRawResponse()
{try{return await RemoteAPI.getJSON('/data/manifest.json');}catch(error){if(error!=404)
throw`Failed to fetch manifest.json with ${error}`;return await RemoteAPI.getJSON('/api/manifest/');}}
static _didFetchManifest(rawResponse)
{Instrumentation.startMeasuringTime('Manifest','_didFetchManifest');const tests=[];const testParentMap={};for(let testId in rawResponse.tests)
tests.push(new Test(testId,rawResponse.tests[testId]));function buildObjectsFromIdMap(idMap,constructor,resolver){for(let id in idMap){if(resolver)
resolver(idMap[id]);new constructor(id,idMap[id]);}}
buildObjectsFromIdMap(rawResponse.metrics,Metric,(raw)=>{raw.test=Test.findById(raw.test);});buildObjectsFromIdMap(rawResponse.platformGroups,PlatformGroup);buildObjectsFromIdMap(rawResponse.all,Platform,(raw)=>{raw.lastModifiedByMetric={};raw.lastModified.forEach((lastModified,index)=>{raw.lastModifiedByMetric[raw.metrics[index]]=lastModified;});raw.metrics=raw.metrics.map((id)=>{return Metric.findById(id);});raw.group=PlatformGroup.findById(raw.group);});buildObjectsFromIdMap(rawResponse.builders,Builder);buildObjectsFromIdMap(rawResponse.repositories,Repository);buildObjectsFromIdMap(rawResponse.bugTrackers,BugTracker,(raw)=>{if(raw.repositories)
raw.repositories=raw.repositories.map((id)=>{return Repository.findById(id);});});buildObjectsFromIdMap(rawResponse.triggerables,Triggerable,(raw)=>{raw.acceptedRepositories=raw.acceptedRepositories.map((repositoryId)=>{return Repository.findById(repositoryId);});raw.repositoryGroups=raw.repositoryGroups.map((group)=>{group.repositories=group.repositories.map((entry)=>{return{repository:Repository.findById(entry.repository),acceptsPatch:entry.acceptsPatch};});return TriggerableRepositoryGroup.ensureSingleton(group.id,group);});raw.configurations=raw.configurations.map((configuration)=>{const[testId,platformId,supportedRepetitionTypes]=configuration;return{test:Test.findById(testId),platform:Platform.findById(platformId),supportedRepetitionTypes};});});if(typeof(UploadedFile)!='undefined')
UploadedFile.fileUploadSizeLimit=rawResponse.fileUploadSizeLimit||0;Instrumentation.endMeasuringTime('Manifest','_didFetchManifest');return{siteTitle:rawResponse.siteTitle,dashboards:rawResponse.dashboards,summaryPages:rawResponse.summaryPages,testAgeToleranceInHours:rawResponse.testAgeToleranceInHours,maxRootReuseAgeInDays:rawResponse.maxRootReuseAgeInDays,}}}
if(typeof module!='undefined')
module.exports.Manifest=Manifest;class ComponentBase extends CommonComponentBase{constructor(name)
{super();this._componentName=name||ComponentBase._componentByClass.get(new.target);const currentlyConstructed=ComponentBase._currentlyConstructedByInterface;let element=currentlyConstructed.get(new.target);if(!element){currentlyConstructed.set(new.target,this);element=document.createElement(this._componentName);currentlyConstructed.delete(new.target);}
element.component=()=>{return this};this._element=element;this._shadow=null;this._actionCallbacks=new Map;this._oldSizeToCheckForResize=null;if(!ComponentBase.useNativeCustomElements)
element.addEventListener('DOMNodeInsertedIntoDocument',()=>this.enqueueToRender());if(!ComponentBase.useNativeCustomElements&&new.target.enqueueToRenderOnResize)
ComponentBase._connectedComponentToRenderOnResize(this);}
element(){return this._element;}
content(id=null)
{this._ensureShadowTree();if(this._shadow&&id!=null)
return this._shadow.getElementById(id);return this._shadow;}
part(id)
{this._ensureShadowTree();if(!this._shadow)
return null;const part=this._shadow.getElementById(id);if(!part)
return null;return part.component();}
dispatchAction(actionName,...args)
{const callback=this._actionCallbacks.get(actionName);if(callback)
return callback.apply(this,args);}
listenToAction(actionName,callback)
{this._actionCallbacks.set(actionName,callback);}
render(){this._ensureShadowTree();}
enqueueToRender()
{Instrumentation.startMeasuringTime('ComponentBase','updateRendering');if(!ComponentBase._componentsToRender){ComponentBase._componentsToRender=new Set;requestAnimationFrame(()=>ComponentBase.renderingTimerDidFire());}
ComponentBase._componentsToRender.add(this);Instrumentation.endMeasuringTime('ComponentBase','updateRendering');}
static renderingTimerDidFire()
{Instrumentation.startMeasuringTime('ComponentBase','renderingTimerDidFire');const componentsToRender=ComponentBase._componentsToRender;this._renderLoop();if(ComponentBase._componentsToRenderOnResize){const resizedComponents=this._resizedComponents(ComponentBase._componentsToRenderOnResize);if(resizedComponents.length){ComponentBase._componentsToRender=new Set(resizedComponents);this._renderLoop();}}
Instrumentation.endMeasuringTime('ComponentBase','renderingTimerDidFire');}
static _renderLoop()
{const componentsToRender=ComponentBase._componentsToRender;do{const currentSet=[...componentsToRender];componentsToRender.clear();const resizeSet=ComponentBase._componentsToRenderOnResize;for(let component of currentSet){Instrumentation.startMeasuringTime('ComponentBase','renderingTimerDidFire.render');component.render();if(resizeSet&&resizeSet.has(component)){const element=component.element();component._oldSizeToCheckForResize={width:element.offsetWidth,height:element.offsetHeight};}
Instrumentation.endMeasuringTime('ComponentBase','renderingTimerDidFire.render');}}while(componentsToRender.size);ComponentBase._componentsToRender=null;}
static _resizedComponents(componentSet)
{if(!componentSet)
return[];const resizedList=[];for(let component of componentSet){const element=component.element();const width=element.offsetWidth;const height=element.offsetHeight;const oldSize=component._oldSizeToCheckForResize;if(oldSize&&oldSize.width==width&&oldSize.height==height)
continue;resizedList.push(component);}
return resizedList;}
static _connectedComponentToRenderOnResize(component)
{if(!ComponentBase._componentsToRenderOnResize){ComponentBase._componentsToRenderOnResize=new Set;window.addEventListener('resize',()=>{const resized=this._resizedComponents(ComponentBase._componentsToRenderOnResize);for(const component of resized)
component.enqueueToRender();});}
ComponentBase._componentsToRenderOnResize.add(component);}
static _disconnectedComponentToRenderOnResize(component)
{ComponentBase._componentsToRenderOnResize.delete(component);}
_ensureShadowTree()
{if(this._shadow)
return;const thisClass=this.__proto__.constructor;let content;let stylesheet;if(!thisClass.hasOwnProperty('_parsed')||!thisClass._parsed){thisClass._parsed=true;const contentTemplate=thisClass['contentTemplate'];if(contentTemplate)
content=ComponentBase._constructNodeTreeFromTemplate(contentTemplate);else if(thisClass.htmlTemplate){const templateElement=document.createElement('template');templateElement.innerHTML=thisClass.htmlTemplate();content=[templateElement.content];}
const styleTemplate=thisClass['styleTemplate'];if(styleTemplate)
stylesheet=ComponentBase._constructStylesheetFromTemplate(styleTemplate);else if(thisClass.cssTemplate)
stylesheet=thisClass.cssTemplate();thisClass._parsedContent=content;thisClass._parsedStylesheet=stylesheet;}else{content=thisClass._parsedContent;stylesheet=thisClass._parsedStylesheet;}
if(!content&&!stylesheet)
return;const shadow=this._element.attachShadow({mode:'closed'});if(content){for(const node of content)
shadow.appendChild(document.importNode(node,true));this._recursivelyUpgradeUnknownElements(shadow,(node)=>{return node instanceof Element?ComponentBase._componentByName.get(node.localName):null;});}
if(stylesheet){const style=document.createElement('style');style.textContent=stylesheet;shadow.appendChild(style);}
this._shadow=shadow;this.didConstructShadowTree();}
didConstructShadowTree(){}
static defineElement(name,elementInterface)
{ComponentBase._componentByName.set(name,elementInterface);ComponentBase._componentByClass.set(elementInterface,name);const enqueueToRenderOnResize=elementInterface.enqueueToRenderOnResize;if(!ComponentBase.useNativeCustomElements)
return;class elementClass extends HTMLElement{constructor()
{super();const currentlyConstructed=ComponentBase._currentlyConstructedByInterface;const component=currentlyConstructed.get(elementInterface);if(component)
return;currentlyConstructed.set(elementInterface,this);new elementInterface();currentlyConstructed.delete(elementInterface);}
connectedCallback()
{this.component().enqueueToRender();if(enqueueToRenderOnResize)
ComponentBase._connectedComponentToRenderOnResize(this.component());}
disconnectedCallback()
{if(enqueueToRenderOnResize)
ComponentBase._disconnectedComponentToRenderOnResize(this.component());}}
const nameDescriptor=Object.getOwnPropertyDescriptor(elementClass,'name');nameDescriptor.value=`${elementInterface.name}Element`;Object.defineProperty(elementClass,'name',nameDescriptor);customElements.define(name,elementClass);}
createEventHandler(callback,options={}){return ComponentBase.createEventHandler(callback,options);}
static createEventHandler(callback,options={})
{return function(event){if(!('preventDefault'in options)||options['preventDefault'])
event.preventDefault();if(!('stopPropagation'in options)||options['stopPropagation'])
event.stopPropagation();callback.call(this,event);};}}
CommonComponentBase._context=document;CommonComponentBase._isNode=(node)=>node instanceof Node;CommonComponentBase._baseClass=ComponentBase;ComponentBase.useNativeCustomElements=!!window.customElements;ComponentBase._componentByName=new Map;ComponentBase._componentByClass=new Map;ComponentBase._currentlyConstructedByInterface=new Map;ComponentBase._componentsToRender=null;ComponentBase._componentsToRenderOnResize=null;class SpinnerIcon extends ComponentBase{constructor()
{super('spinner-icon');}
static cssTemplate()
{return` :host{display:inline-block;width:2rem;height:2rem;will-change:opacity}line{animation:spinner-animation 1.6s linear infinite;stroke:rgb(230,230,230);stroke-width:10;stroke-linecap:round}line:nth-child(0){animation-delay:0.0s}line:nth-child(1){animation-delay:0.2s}line:nth-child(2){animation-delay:0.4s}line:nth-child(3){animation-delay:0.6s}line:nth-child(4){animation-delay:0.8s}line:nth-child(5){animation-delay:1.0s}line:nth-child(6){animation-delay:1.2s}line:nth-child(7){animation-delay:1.4s}@keyframes spinner-animation{0%{stroke:rgb(25,25,25)}50%{stroke:rgb(230,230,230)}}`;}
static htmlTemplate()
{return`<svg class="spinner" viewBox="0 0 100 100"> <line x1="10" y1="50" x2="30" y2="50"/> <line x1="21.72" y1="21.72" x2="35.86" y2="35.86"/> <line x1="50" y1="10" x2="50" y2="30"/> <line x1="78.28" y1="21.72" x2="64.14" y2="35.86"/> <line x1="70" y1="50" x2="90" y2="50"/> <line x1="65.86" y1="65.86" x2="78.28" y2="78.28"/> <line x1="50" y1="70" x2="50" y2="90"/> <line x1="21.72" y1="78.28" x2="35.86" y2="65.86"/> </svg>`;}}
ComponentBase.defineElement('spinner-icon',SpinnerIcon);class ButtonBase extends ComponentBase{constructor(name)
{super(name);this._disabled=false;this._title=null;}
setDisabled(disabled)
{this._disabled=disabled;this.enqueueToRender();}
setButtonTitle(title)
{this._title=title;this.enqueueToRender();}
didConstructShadowTree()
{this.content('button').addEventListener('click',this.createEventHandler(()=>{this.dispatchAction('activate');}));}
render()
{super.render();if(this._disabled)
this.content('button').setAttribute('disabled','');else
this.content('button').removeAttribute('disabled');if(this._title)
this.content('button').setAttribute('title',this._title);else
this.content('button').removeAttribute('title');}
static htmlTemplate()
{return`<a id="button" href="#"><svg viewBox="0 0 100 100">${this.buttonContent()}</svg></a>`;}
static buttonContent(){throw'NotImplemented';}
static sizeFactor(){return 1;}
static cssTemplate()
{const sizeFactor=this.sizeFactor();return` :host{display:inline-block;width:${sizeFactor}rem;height:${sizeFactor}rem}a{vertical-align:bottom;display:block;opacity:0.3}a:hover{opacity:0.6}a[disabled]{pointer-events:none;cursor:default;opacity:0.2}svg{display:block}`;}}
class WarningIcon extends ButtonBase{constructor()
{super('warning-icon');}
render()
{super.render();}
setWarning(warning)
{this.setButtonTitle(warning);}
static sizeFactor(){return 0.7;}
static buttonContent()
{return`<g stroke="#9f6000" fill="#9f6000" stroke-width="7">
                <polygon points="0,0, 100,0, 0,100" />
            </g>`;}}
ComponentBase.defineElement('warning-icon',WarningIcon);class CloseButton extends ButtonBase{constructor()
{super('close-button');}
static buttonContent()
{return`<g stroke="black" stroke-width="10">
            <circle cx="50" cy="50" r="45" fill="transparent"/>
            <polygon points="30,30 70,70" />
            <polygon points="30,70 70,30" />
        </g>`;}}
ComponentBase.defineElement('close-button',CloseButton);class CommitLogViewer extends ComponentBase{constructor()
{super('commit-log-viewer');this._lazilyFetchCommitLogs=new LazilyEvaluatedFunction(this._fetchCommitLogs.bind(this));this._repository=null;this._fetchingPromise=null;this._commits=null;this._precedingCommit=null;this._renderCommitListLazily=new LazilyEvaluatedFunction(this._renderCommitList.bind(this));this._showRepositoryName=true;}
setShowRepositoryName(shouldShow)
{this._showRepositoryName=shouldShow;this.enqueueToRender();}
view(repository,precedingRevision,lastRevision)
{this._lazilyFetchCommitLogs.evaluate(repository,precedingRevision,lastRevision);}
_fetchCommitLogs(repository,precedingRevision,lastRevision)
{this._fetchingPromise=null;this._commits=null;if(!repository||!lastRevision){this._repository=null;this.enqueueToRender();return;}
let promise;const fetchSingleCommit=!precedingRevision||precedingRevision==lastRevision;if(fetchSingleCommit){promise=CommitLog.fetchForSingleRevision(repository,lastRevision).then((commits)=>{if(this._fetchingPromise!=promise)
return;this._commits=commits;this._precedingCommit=null;});}else{promise=Promise.all([CommitLog.fetchBetweenRevisions(repository,precedingRevision,lastRevision).then((commits)=>{if(this._fetchingPromise!=promise)
return;this._commits=commits;}),CommitLog.fetchForSingleRevision(repository,precedingRevision).then((precedingCommit)=>{if(this._fetchingPromise!=promise)
return;this._precedingCommit=precedingCommit[0];})]);}
this._repository=repository;this._fetchingPromise=promise;this._fetchingPromise.then((commits)=>{if(this._fetchingPromise!=promise)
return;this._fetchingPromise=null;this.enqueueToRender();},(error)=>{if(this._fetchingPromise!=promise)
return;this._fetchingPromise=null;this._commits=null;this._precedingCommit=null;this.enqueueToRender();});this.enqueueToRender();}
render()
{const shouldShowRepositoryName=this._repository&&(this._commits||this._fetchingPromise)&&this._showRepositoryName;this.content('repository-name').textContent=shouldShowRepositoryName?this._repository.name():'';this.content('spinner-container').style.display=this._fetchingPromise?null:'none';this._renderCommitListLazily.evaluate(this._commits,this._precedingCommit);}
_renderCommitList(commits,precedingCommit)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;commits=commits&&precedingCommit&&precedingCommit.ownsCommits()?[precedingCommit].concat(commits):commits;let previousCommit=null;this.renderReplace(this.content('commits-list'),(commits||[]).map((commit)=>{const label=commit.label();const url=commit.url();const ownsCommits=previousCommit&&previousCommit.ownsCommits()&&commit.ownsCommits();const ownedCommitDifferenceRow=ownsCommits?element('tr',element('td',{colspan:2},new OwnedCommitViewer(previousCommit,commit))):[];previousCommit=commit;return[ownedCommitDifferenceRow,element('tr',[element('th',[element('h4',{class:'revision'},url?link(label,commit.title(),url):label),commit.author()||'']),element('td',commit.message()?commit.message().substring(0,80):'')])];}));}
static htmlTemplate()
{return` <div class="commits-viewer-container"> <div id="spinner-container"><spinner-icon id="spinner"></spinner-icon></div> <table id="commits-viewer-table"> <caption id="repository-name"></caption> <tbody id="commits-list"></tbody> </table> </div> `;}
static cssTemplate()
{return` .commits-viewer-container{width:100%;height:calc(100% - 2px);overflow-y:scroll}#commits-viewer-table{width:100%;border-collapse:collapse;border-bottom:solid 1px #ccc}caption{font-weight:inherit;font-size:1rem;text-align:center;padding:0.2rem}.revision{white-space:nowrap;font-weight:normal;margin:0;padding:0}td,th{word-break:break-word;border-top:solid 1px #ccc;padding:0.2rem;margin:0;font-size:0.8rem;font-weight:normal}#spinner-container{margin-top:2rem;text-align:center}`;}}
ComponentBase.defineElement('commit-log-viewer',CommitLogViewer);class OwnedCommitViewer extends ComponentBase{constructor(previousCommit,currentCommit)
{super('owned-commit-viewer');this._previousCommit=previousCommit;this._currentCommit=currentCommit;this._previousOwnedCommits=null;this._currentOwnedCommits=null;this._showingOwnedCommits=false;this._renderOwnedCommitTableLazily=new LazilyEvaluatedFunction(this._renderOwnedCommitTable.bind(this));}
didConstructShadowTree()
{this.part('expand-collapse').listenToAction('toggle',(expanded)=>this._toggleVisibility(expanded));}
_toggleVisibility(expanded)
{this._showingOwnedCommits=expanded;this.enqueueToRender();Promise.all([this._previousCommit.fetchOwnedCommits(),this._currentCommit.fetchOwnedCommits()]).then((ownedCommitsList)=>{this._previousOwnedCommits=ownedCommitsList[0];this._currentOwnedCommits=ownedCommitsList[1];this.enqueueToRender();});}
render()
{const hideSpinner=(this._previousOwnedCommits&&this._currentOwnedCommits)||!this._showingOwnedCommits;this.content('difference-entries').style.display=this._showingOwnedCommits?null:'none';this.content('spinner-container').style.display=hideSpinner?'none':null;this.content('difference-table').style.display=this._showingOwnedCommits?null:'none';this._renderOwnedCommitTableLazily.evaluate(this._previousOwnedCommits,this._currentOwnedCommits);}
_renderOwnedCommitTable(previousOwnedCommits,currentOwnedCommits)
{if(!previousOwnedCommits||!currentOwnedCommits)
return;const difference=CommitLog.ownedCommitDifferenceForOwnerCommits(this._previousCommit,this._currentCommit);const sortedRepositories=Repository.sortByName([...difference.keys()]);const element=ComponentBase.createElement;const tableEntries=sortedRepositories.map((repository)=>{const revisions=difference.get(repository);return element('tr',[element('td',repository.name()),element('td',revisions[0]?revisions[0].revision():''),element('td',revisions[1]?revisions[1].revision():'')]);});this.renderReplace(this.content('difference-entries'),tableEntries);}
static htmlTemplate()
{return` <expand-collapse-button id="expand-collapse"></expand-collapse-button> <table id="difference-table"> <tbody id="difference-entries"></tbody> </table> <div id="spinner-container"><spinner-icon id="spinner"></spinner-icon></div>`;}
static cssTemplate(){return` :host{display:block;font-size:0.8rem;font-weight:normal}expand-collapse-button{margin-left:calc(50% - 0.8rem);display:block}td,th{padding:0.2rem;margin:0;border-top:solid 1px #ccc}#difference-table{width:100%}#spinner-container{text-align:center}`;}}
ComponentBase.defineElement('owned-commit-viewer',OwnedCommitViewer);class EditableText extends ComponentBase{constructor(text)
{super('editable-text');this._text=text;this._inEditingMode=false;this._updatingPromise=null;this._actionLink=this.content().querySelector('.editable-text-action a');this._label=this.content().querySelector('.editable-text-label');}
didConstructShadowTree()
{const button=this.content('action-button');button.addEventListener('mousedown',this.createEventHandler(()=>this._didClick()));button.addEventListener('click',this.createEventHandler(()=>this._didClick()));this.element().addEventListener('blur',()=>{if(!this.content().activeElement)
this._endEditingMode();});}
editedText(){return this.content('text-field').value;}
text(){return this._text;}
setText(text)
{this._text=text;this.enqueueToRender();}
render()
{const label=this.content('label');label.textContent=this._text;label.style.display=this._inEditingMode?'none':null;const textField=this.content('text-field');textField.style.display=this._inEditingMode?null:'none';textField.readOnly=!!this._updatingPromise;this.content('action-button').textContent=this._inEditingMode?(this._updatingPromise?'...':'Save'):'Edit';this.content('action-button-container').style.display=this._text?null:'none';if(this._inEditingMode){if(!this._updatingPromise)
textField.focus();}}
_didClick()
{if(this._updatingPromise)
return;if(this._inEditingMode){const result=this.dispatchAction('update');if(result instanceof Promise)
this._updatingPromise=result.then(()=>this._endEditingMode());else
this._endEditingMode();}else{this._inEditingMode=true;const textField=this.content('text-field');textField.value=this._text;textField.style.width=(this._text.length/1.5)+'rem';this.enqueueToRender();}}
_endEditingMode()
{this._inEditingMode=false;this._updatingPromise=null;this.enqueueToRender();}
static htmlTemplate()
{return` <span id="container"> <input id="text-field" type="text"> <span id="label"></span> <span id="action-button-container">(<a id="action-button" href="#">Edit</a>)</span> </span>`;}
static cssTemplate()
{return` #container{position:relative;padding-right:2.5rem}#text-field{background:transparent;margin:0;padding:0;color:inherit;font-weight:inherit;font-size:inherit;border:none}#action-button-container{position:absolute;right:0;padding-left:0.2rem;color:#999;font-size:0.8rem;top:50%;margin-top:-0.4rem;vertical-align:middle}#action-button{color:inherit;text-decoration:none}`;}}
ComponentBase.defineElement('editable-text',EditableText);class ExpandCollapseButton extends ButtonBase{constructor()
{super('expand-collapse-button');this._expanded=false;}
didConstructShadowTree()
{super.didConstructShadowTree();this.listenToAction('activate',()=>{this._expanded=!this._expanded;this.content('button').className=this._expanded?'expanded':null;this.enqueueToRender();this.dispatchAction('toggle',this._expanded);});}
static sizeFactor(){return 0.8;}
static buttonContent()
{return`<g stroke="#333" fill="none" stroke-width="10">
            <polyline points="0,25 50,50 100,25"/>
            <polyline points="0,50 50,75 100,50"/>
        </g>`;}
static cssTemplate()
{return super.cssTemplate()+` a.expanded{transform:rotate(180deg)}`;}}
ComponentBase.defineElement('expand-collapse-button',ExpandCollapseButton);class TimeSeriesChart extends ComponentBase{constructor(sourceList,options)
{super();this._canvas=null;this._sourceList=sourceList;this._trendLines=null;this._options=options;this._fetchedTimeSeries=null;this._sampledTimeSeriesData=null;this._renderedTrendLines=false;this._valueRangeCache=null;this._annotations=null;this._annotationRows=null;this._startTime=null;this._endTime=null;this._width=null;this._height=null;this._contextScaleX=1;this._contextScaleY=1;this._rem=null;}
_ensureCanvas()
{if(!this._canvas){this._canvas=this._createCanvas();this._canvas.style.display='block';this._canvas.style.position='absolute';this._canvas.style.left='0px';this._canvas.style.top='0px';this.content().appendChild(this._canvas);}
return this._canvas;}
static cssTemplate()
{return` :host{display:block!important;position:relative!important}`;}
static get enqueueToRenderOnResize(){return true;}
_createCanvas()
{return document.createElement('canvas');}
setDomain(startTime,endTime)
{console.assert(startTime<endTime,'startTime must be before endTime');this._startTime=startTime;this._endTime=endTime;this._fetchedTimeSeries=null;this.fetchMeasurementSets(false);}
setSourceList(sourceList)
{this._sourceList=sourceList;this._fetchedTimeSeries=null;this.fetchMeasurementSets(false);}
sourceList(){return this._sourceList;}
clearTrendLines()
{this._trendLines=null;this._renderedTrendLines=false;this.enqueueToRender();}
setTrendLine(sourceIndex,trendLine)
{if(this._trendLines)
this._trendLines=this._trendLines.slice(0);else
this._trendLines=[];this._trendLines[sourceIndex]=trendLine;this._renderedTrendLines=false;this.enqueueToRender();}
fetchMeasurementSets(noCache)
{var fetching=false;for(var source of this._sourceList){if(source.measurementSet){if(!noCache&&source.measurementSet.hasFetchedRange(this._startTime,this._endTime))
continue;source.measurementSet.fetchBetween(this._startTime,this._endTime,this._didFetchMeasurementSet.bind(this,source.measurementSet),noCache);fetching=true;}}
this._sampledTimeSeriesData=null;this._valueRangeCache=null;this._annotationRows=null;if(!fetching)
this.enqueueToRender();}
_didFetchMeasurementSet(set)
{this._fetchedTimeSeries=null;this._sampledTimeSeriesData=null;this._valueRangeCache=null;this._annotationRows=null;this.enqueueToRender();}
sampledTimeSeriesData(type)
{if(!this._sampledTimeSeriesData)
return null;for(var i=0;i<this._sourceList.length;i++){if(this._sourceList[i].type==type)
return this._sampledTimeSeriesData[i];}
return null;}
referencePoints(type)
{const view=this.sampledTimeSeriesData(type);if(!view||!this._startTime||!this._endTime)
return null;const point=view.lastPointInTimeRange(this._startTime,this._endTime);if(!point)
return null;return{view,currentPoint:point,previousPoint:null};}
setAnnotations(annotations)
{this._annotations=annotations;this._annotationRows=null;this.enqueueToRender();}
render()
{if(!this._startTime||!this._endTime)
return;var canvas=this._ensureCanvas();var metrics=this._layout();if(!metrics.doneWork)
return;Instrumentation.startMeasuringTime('TimeSeriesChart','render');var context=canvas.getContext('2d');context.scale(this._contextScaleX,this._contextScaleY);context.clearRect(0,0,this._width,this._height);context.font=metrics.fontSize+'px sans-serif';const axis=this._options.axis;if(axis){context.strokeStyle=axis.gridStyle;context.lineWidth=1/this._contextScaleY;this._renderXAxis(context,metrics,this._startTime,this._endTime);this._renderYAxis(context,metrics,this._valueRangeCache[0],this._valueRangeCache[1]);}
context.save();context.beginPath();context.rect(metrics.chartX,metrics.chartY,metrics.chartWidth,metrics.chartHeight);context.clip();this._renderChartContent(context,metrics);context.restore();context.setTransform(1,0,0,1,0,0);Instrumentation.endMeasuringTime('TimeSeriesChart','render');}
_layout()
{var doneWork=this._updateCanvasSizeIfClientSizeChanged();var metrics=this._computeHorizontalRenderingMetrics();doneWork|=this._ensureSampledTimeSeries(metrics);doneWork|=this._ensureTrendLines();doneWork|=this._ensureValueRangeCache();this._computeVerticalRenderingMetrics(metrics);doneWork|=this._layoutAnnotationBars(metrics);metrics.doneWork=doneWork;return metrics;}
_computeHorizontalRenderingMetrics()
{if(!this._rem)
this._rem=parseFloat(getComputedStyle(document.documentElement).fontSize);var timeDiff=this._endTime-this._startTime;var startTime=this._startTime;const axis=this._options.axis||{};const fontSize=(axis.fontSize||1)*this._rem;const chartX=(axis.yAxisWidth||0)*fontSize;var chartY=0;var chartWidth=this._width-chartX;var chartHeight=this._height-(axis.xAxisHeight||0)*fontSize;if(axis.xAxisEndPadding)
timeDiff+=axis.xAxisEndPadding/(chartWidth/timeDiff);return{xToTime:function(x)
{var time=(x-chartX)/(chartWidth/timeDiff)+ +startTime;console.assert(Math.abs(x-this.timeToX(time))<1e-6);return time;},timeToX:function(time){return(chartWidth/timeDiff)*(time-startTime)+chartX;},valueToY:function(value)
{return((chartHeight-this.annotationHeight)/this.valueDiff)*(this.endValue-value)+chartY;},chartX:chartX,chartY:chartY,chartWidth:chartWidth,chartHeight:chartHeight,annotationHeight:0,fontSize:fontSize,valueDiff:0,endValue:0,};}
_computeVerticalRenderingMetrics(metrics)
{var minValue=this._valueRangeCache[0];var maxValue=this._valueRangeCache[1];var valueDiff=maxValue-minValue;var valueMargin=valueDiff*0.05;var endValue=maxValue+valueMargin;var valueDiffWithMargin=valueDiff+valueMargin*2;metrics.valueDiff=valueDiffWithMargin;metrics.endValue=endValue;}
_layoutAnnotationBars(metrics)
{if(!this._annotations||!this._options.annotations)
return false;var barHeight=this._options.annotations.barHeight;var barSpacing=this._options.annotations.barSpacing;if(this._annotationRows){metrics.annotationHeight=this._annotationRows.length*(barHeight+barSpacing);return false;}
Instrumentation.startMeasuringTime('TimeSeriesChart','layoutAnnotationBars');var minWidth=this._options.annotations.minWidth;this._annotations.forEach(function(annotation){var x1=metrics.timeToX(annotation.startTime);var x2=metrics.timeToX(annotation.endTime);if(x2-x1<minWidth){x1-=minWidth/2;x2+=minWidth/2;}
annotation.x=x1;annotation.width=x2-x1;});var sortedAnnotations=this._annotations.sort(function(a,b){return a.x-b.x});
var rows=[];sortedAnnotations.forEach(function(currentItem){for(var rowIndex=0;rowIndex<rows.length;rowIndex++){var currentRow=rows[rowIndex];var lastItem=currentRow[currentRow.length-1];if(lastItem.x+lastItem.width+minWidth<currentItem.x){currentRow.push(currentItem);return;}}
rows.push([currentItem]);});this._annotationRows=rows;for(var rowIndex=0;rowIndex<rows.length;rowIndex++){for(var annotation of rows[rowIndex]){annotation.y=metrics.chartY+metrics.chartHeight-(rows.length-rowIndex)*(barHeight+barSpacing);annotation.height=barHeight;}}
metrics.annotationHeight=rows.length*(barHeight+barSpacing);Instrumentation.endMeasuringTime('TimeSeriesChart','layoutAnnotationBars');return true;}
_renderXAxis(context,metrics,startTime,endTime)
{var typicalWidth=context.measureText('12/31 x').width;var maxXAxisLabels=Math.floor(metrics.chartWidth/typicalWidth);var xAxisGrid=TimeSeriesChart.computeTimeGrid(startTime,endTime,maxXAxisLabels);for(var item of xAxisGrid){context.beginPath();var x=metrics.timeToX(item.time);context.moveTo(x,metrics.chartY);context.lineTo(x,metrics.chartY+metrics.chartHeight);context.stroke();}
if(!this._options.axis.xAxisHeight)
return;var rightEdgeOfPreviousItem=0;for(var item of xAxisGrid){var xCenter=metrics.timeToX(item.time);var width=context.measureText(item.label).width;var x=xCenter-width/2;if(x+width>metrics.chartX+metrics.chartWidth){x=metrics.chartX+metrics.chartWidth-width;if(x<=rightEdgeOfPreviousItem)
break;}
rightEdgeOfPreviousItem=x+width;context.fillText(item.label,x,metrics.chartY+metrics.chartHeight+metrics.fontSize);}}
_renderYAxis(context,metrics,minValue,maxValue)
{const maxYAxisLabels=Math.floor(metrics.chartHeight/metrics.fontSize/2);const yAxisGrid=TimeSeriesChart.computeValueGrid(minValue,maxValue,maxYAxisLabels,this._options.axis.valueFormatter);for(let item of yAxisGrid){context.beginPath();const y=metrics.valueToY(item.value);context.moveTo(metrics.chartX,y);context.lineTo(metrics.chartX+metrics.chartWidth,y);context.stroke();}
if(!this._options.axis.yAxisWidth)
return;for(let item of yAxisGrid){const x=(metrics.chartX-context.measureText(item.label).width)/2;let y=metrics.valueToY(item.value)+metrics.fontSize/2.5;if(y<metrics.fontSize)
y=metrics.fontSize;context.fillText(item.label,x,y);}}
_renderChartContent(context,metrics)
{context.lineJoin='round';for(var i=0;i<this._sourceList.length;i++){var source=this._sourceList[i];var series=this._sampledTimeSeriesData[i];if(series)
this._renderTimeSeries(context,metrics,source,series,this._trendLines&&this._trendLines[i]?'background':'');}
for(var i=0;i<this._sourceList.length;i++){var source=this._sourceList[i];var trendLine=this._trendLines?this._trendLines[i]:null;if(series&&trendLine)
this._renderTimeSeries(context,metrics,source,trendLine,'foreground');}
if(!this._annotationRows)
return;for(var row of this._annotationRows){for(var bar of row){if(bar.x>this.chartWidth||bar.x+bar.width<0)
continue;context.fillStyle=bar.fillStyle;context.fillRect(bar.x,bar.y,bar.width,bar.height);}}}
_renderTimeSeries(context,metrics,source,series,layerName)
{for(let point=series.firstPoint();point;point=series.nextPoint(point)){point.x=metrics.timeToX(point.time);point.y=metrics.valueToY(point.value);}
if(source.intervalStyle){context.strokeStyle=source.intervalStyle;context.fillStyle=source.intervalStyle;context.lineWidth=source.intervalWidth;context.beginPath();var width=1;for(var i=0;i<series.length;i++){var point=series[i];var interval=point.interval;var value=interval?interval[0]:point.value;context.lineTo(point.x-width,metrics.valueToY(value));context.lineTo(point.x+width,metrics.valueToY(value));}
for(var i=series.length-1;i>=0;i--){var point=series[i];var interval=point.interval;var value=interval?interval[1]:point.value;context.lineTo(point.x+width,metrics.valueToY(value));context.lineTo(point.x-width,metrics.valueToY(value));}
context.fill();}
context.strokeStyle=this._sourceOptionWithFallback(source,layerName+'LineStyle','lineStyle');context.lineWidth=this._sourceOptionWithFallback(source,layerName+'LineWidth','lineWidth');context.beginPath();for(let point=series.firstPoint();point;point=series.nextPoint(point))
context.lineTo(point.x,point.y);context.stroke();context.fillStyle=this._sourceOptionWithFallback(source,layerName+'PointStyle','pointStyle');var radius=this._sourceOptionWithFallback(source,layerName+'PointRadius','pointRadius');if(radius){for(let point=series.firstPoint();point;point=series.nextPoint(point))
this._fillCircle(context,point.x,point.y,radius);}}
_sourceOptionWithFallback(option,preferred,fallback)
{return preferred in option?option[preferred]:option[fallback];}
_fillCircle(context,cx,cy,radius)
{context.beginPath();context.arc(cx,cy,radius,0,2*Math.PI);context.fill();}
_ensureFetchedTimeSeries()
{if(this._fetchedTimeSeries)
return false;Instrumentation.startMeasuringTime('TimeSeriesChart','ensureFetchedTimeSeries');this._fetchedTimeSeries=this._sourceList.map(function(source){return source.measurementSet.fetchedTimeSeries(source.type,source.includeOutliers,source.extendToFuture);});Instrumentation.endMeasuringTime('TimeSeriesChart','ensureFetchedTimeSeries');return true;}
_ensureSampledTimeSeries(metrics)
{if(this._sampledTimeSeriesData)
return false;this._ensureFetchedTimeSeries();Instrumentation.startMeasuringTime('TimeSeriesChart','ensureSampledTimeSeries');const startTime=this._startTime;const endTime=this._endTime;this._sampledTimeSeriesData=this._sourceList.map((source,sourceIndex)=>{const timeSeries=this._fetchedTimeSeries[sourceIndex];if(!timeSeries)
return null;const pointAfterStart=timeSeries.findPointAfterTime(startTime);const pointBeforeStart=(pointAfterStart?timeSeries.previousPoint(pointAfterStart):null)||timeSeries.firstPoint();const pointAfterEnd=timeSeries.findPointAfterTime(endTime)||timeSeries.lastPoint();if(!pointBeforeStart||!pointAfterEnd)
return null;const view=timeSeries.viewBetweenPoints(pointBeforeStart,pointAfterEnd);if(!source.sampleData)
return view;const viewWidth=Math.min(metrics.chartWidth,metrics.timeToX(pointAfterEnd.time)-metrics.timeToX(pointBeforeStart.time));const maximumNumberOfPoints=2*viewWidth/(source.pointRadius||2);return this._sampleTimeSeries(view,maximumNumberOfPoints,new Set);});Instrumentation.endMeasuringTime('TimeSeriesChart','ensureSampledTimeSeries');this.dispatchAction('dataChange');return true;}
_sampleTimeSeries(view,maximumNumberOfPoints,excludedPoints)
{if(view.length()<2||maximumNumberOfPoints>=view.length()||maximumNumberOfPoints<1)
return view;Instrumentation.startMeasuringTime('TimeSeriesChart','sampleTimeSeries');let ranks=new Array(view.length());let i=0;for(let point of view){let previousPoint=view.previousPoint(point)||point;let nextPoint=view.nextPoint(point)||point;ranks[i]=nextPoint.time-previousPoint.time;i++;}
const sortedRanks=ranks.slice(0).sort((a,b)=>b-a);const minimumRank=sortedRanks[Math.floor(maximumNumberOfPoints)];const sampledData=view.filter((point,i)=>{return excludedPoints.has(point.id)||ranks[i]>=minimumRank;});Instrumentation.endMeasuringTime('TimeSeriesChart','sampleTimeSeries');Instrumentation.reportMeasurement('TimeSeriesChart','samplingRatio','%',sampledData.length()/view.length()*100);return sampledData;}
_ensureTrendLines()
{if(this._renderedTrendLines)
return false;this._renderedTrendLines=true;return true;}
_ensureValueRangeCache()
{if(this._valueRangeCache)
return false;Instrumentation.startMeasuringTime('TimeSeriesChart','valueRangeCache');var startTime=this._startTime;var endTime=this._endTime;var min;var max;for(var seriesData of this._sampledTimeSeriesData){if(!seriesData)
continue;for(let point=seriesData.firstPoint();point;point=seriesData.nextPoint(point)){var minCandidate=point.interval?point.interval[0]:point.value;var maxCandidate=point.interval?point.interval[1]:point.value;min=(min===undefined)?minCandidate:Math.min(min,minCandidate);max=(max===undefined)?maxCandidate:Math.max(max,maxCandidate);}}
if(min==max)
max=max*1.1;this._valueRangeCache=[min,max];Instrumentation.endMeasuringTime('TimeSeriesChart','valueRangeCache');return true;}
_updateCanvasSizeIfClientSizeChanged()
{var canvas=this._ensureCanvas();var newWidth=this.element().clientWidth;var newHeight=this.element().clientHeight;if(!newWidth||!newHeight||(newWidth==this._width&&newHeight==this._height))
return false;var scale=window.devicePixelRatio;canvas.width=newWidth*scale;canvas.height=newHeight*scale;canvas.style.width=newWidth+'px';canvas.style.height=newHeight+'px';this._contextScaleX=scale;this._contextScaleY=scale;this._width=newWidth;this._height=newHeight;this._sampledTimeSeriesData=null;this._annotationRows=null;return true;}
static computeTimeGrid(min,max,maxLabels)
{const diffPerLabel=(max-min)/maxLabels;let iterator;for(iterator of this._timeIterators()){if(iterator.diff>=diffPerLabel)
break;}
console.assert(iterator);const currentTime=new Date(min);currentTime.setUTCMilliseconds(0);currentTime.setUTCSeconds(0);currentTime.setUTCMinutes(0);iterator.next(currentTime);const fitsInOneDay=max-min<24*3600*1000;let result=[];let previousDate=null;let previousMonth=null;while(currentTime<=max&&result.length<maxLabels){const time=new Date(currentTime);const month=time.getUTCMonth()+1;const date=time.getUTCDate();const hour=time.getUTCHours();const hourLabel=((hour%12)||12)+(hour>=12?'PM':'AM');iterator.next(currentTime);let label;const isMidnight=!hour;if((date==previousDate&&month==previousMonth)||((!isMidnight||previousDate==null)&&fitsInOneDay))
label=hourLabel;else{label=`${month}/${date}`;if(!isMidnight&&currentTime.getUTCDate()!=date)
label+=' '+hourLabel;}
result.push({time:time,label:label});previousDate=date;previousMonth=month;}
console.assert(result.length<=maxLabels);return result;}
static _timeIterators()
{var HOUR=3600*1000;var DAY=24*HOUR;return[{diff:2*HOUR,next:function(date){if(date.getUTCHours()>=22){date.setUTCHours(0);date.setUTCDate(date.getUTCDate()+1);}else
date.setUTCHours(Math.floor(date.getUTCHours()/2)*2+2);},},{diff:12*HOUR,next:function(date){if(date.getUTCHours()>=12){date.setUTCHours(0);date.setUTCDate(date.getUTCDate()+1);}else
date.setUTCHours(12);},},{diff:DAY,next:function(date){date.setUTCHours(0);date.setUTCDate(date.getUTCDate()+1);}},{diff:2*DAY,next:function(date){date.setUTCHours(0);date.setUTCDate(date.getUTCDate()+2);}},{diff:7*DAY,next:function(date){date.setUTCHours(0);if(date.getUTCDay())
date.setUTCDate(date.getUTCDate()+(7-date.getUTCDay()));else
date.setUTCDate(date.getUTCDate()+7);}},{diff:16*DAY,next:function(date){date.setUTCHours(0);if(date.getUTCDate()>=15){date.setUTCMonth(date.getUTCMonth()+1);date.setUTCDate(1);}else
date.setUTCDate(15);}},{diff:31*DAY,next:function(date){date.setUTCHours(0);const dayOfMonth=date.getUTCDate();if(dayOfMonth>1&&dayOfMonth<15)
date.setUTCDate(15);else{if(dayOfMonth!=15)
date.setUTCDate(1);date.setUTCMonth(date.getUTCMonth()+1);}}},{diff:60*DAY,next:function(date){date.setUTCHours(0);date.setUTCDate(1);date.setUTCMonth(date.getUTCMonth()+2);}},{diff:90*DAY,next:function(date){date.setUTCHours(0);date.setUTCDate(1);date.setUTCMonth(date.getUTCMonth()+3);}},{diff:120*DAY,next:function(date){date.setUTCHours(0);date.setUTCDate(1);date.setUTCMonth(date.getUTCMonth()+4);}},];}
static computeValueGrid(min,max,maxLabels,formatter)
{const diff=max-min;if(!diff)
return[];const diffPerLabel=diff/maxLabels;const maxAbsValue=Math.max(Math.abs(min),Math.abs(max));let scalingFactor=1;const divisor=formatter.divisor;while(maxAbsValue*scalingFactor<1)
scalingFactor*=formatter.divisor;while(maxAbsValue*scalingFactor>divisor)
scalingFactor/=formatter.divisor;const scaledDiff=diffPerLabel*scalingFactor;
const digitsInScaledDiff=Math.ceil(Math.log(scaledDiff)/Math.log(10));let step=Math.pow(10,digitsInScaledDiff);if(step/5>=scaledDiff)
step/=5; else if(step/2>=scaledDiff)
step/=2
 step/=scalingFactor;const gridValues=[];let currentValue=Math.ceil(min/step)*step;while(currentValue<=max){let unscaledValue=currentValue;gridValues.push({value:unscaledValue,label:formatter(unscaledValue,maxAbsValue)});currentValue+=step;}
return gridValues;}}
ComponentBase.defineElement('time-series-chart',TimeSeriesChart);class InteractiveTimeSeriesChart extends TimeSeriesChart{constructor(sourceList,options)
{super(sourceList,options);this._indicatorID=null;this._indicatorIsLocked=false;this._currentAnnotation=null;this._forceRender=false;this._lastMouseDownLocation=null;this._dragStarted=false;this._didEndDrag=false;this._selectionTimeRange=null;this._renderedSelection=null;this._annotationLabel=null;this._renderedAnnotation=null;}
currentIndicator()
{var id=this._indicatorID;if(!id)
return null;if(!this._sampledTimeSeriesData)
return null;for(var view of this._sampledTimeSeriesData){if(!view)
continue;let point=view.findById(id);if(!point)
continue;return{view,point,isLocked:this._indicatorIsLocked};}
return null;}
currentSelection(){return this._selectionTimeRange;}
selectedPoints(type)
{const selection=this._selectionTimeRange;const data=this.sampledTimeSeriesData(type);return selection&&data?data.viewTimeRange(selection[0],selection[1]):null;}
firstSelectedPoint(type)
{const selection=this._selectionTimeRange;const data=this.sampledTimeSeriesData(type);return selection&&data?data.firstPointInTimeRange(selection[0],selection[1]):null;}
referencePoints(type)
{const selection=this.currentSelection();if(selection){const view=this.selectedPoints('current');if(!view)
return null;const firstPoint=view.lastPoint();const lastPoint=view.firstPoint();if(!firstPoint)
return null;return{view,currentPoint:firstPoint,previousPoint:firstPoint!=lastPoint?lastPoint:null};}else{const indicator=this.currentIndicator();if(!indicator)
return null;return{view:indicator.view,currentPoint:indicator.point,previousPoint:indicator.view.previousPoint(indicator.point)};}
return null;}
setIndicator(id,shouldLock)
{var selectionDidChange=!!this._sampledTimeSeriesData;this._indicatorID=id;this._indicatorIsLocked=shouldLock;this._lastMouseDownLocation=null;this._selectionTimeRange=null;this._forceRender=true;if(selectionDidChange)
this._notifySelectionChanged(false);}
moveLockedIndicatorWithNotification(forward)
{const indicator=this.currentIndicator();if(!indicator||!indicator.isLocked)
return false;console.assert(!this._selectionTimeRange);const constrainedView=indicator.view.viewTimeRange(this._startTime,this._endTime);const newPoint=forward?constrainedView.nextPoint(indicator.point):constrainedView.previousPoint(indicator.point);if(!newPoint||this._indicatorID==newPoint.id)
return false;this._indicatorID=newPoint.id;this._lastMouseDownLocation=null;this._forceRender=true;this.enqueueToRender();this._notifyIndicatorChanged();}
setSelection(newSelectionTimeRange)
{var indicatorDidChange=!!this._indicatorID;this._indicatorID=null;this._indicatorIsLocked=false;this._lastMouseDownLocation=null;this._selectionTimeRange=newSelectionTimeRange;this._forceRender=true;if(indicatorDidChange)
this._notifyIndicatorChanged();}
_createCanvas()
{var canvas=super._createCanvas();canvas.addEventListener('mousemove',this._mouseMove.bind(this));canvas.addEventListener('mouseleave',this._mouseLeave.bind(this));canvas.addEventListener('mousedown',this._mouseDown.bind(this));window.addEventListener('mouseup',this._mouseUp.bind(this));canvas.addEventListener('click',this._click.bind(this));this._annotationLabel=this.content('annotation-label');this._zoomButton=this.content('zoom-button');this._zoomButton.onclick=(event)=>{event.preventDefault();this.dispatchAction('zoom',this._selectionTimeRange);}
return canvas;}
static htmlTemplate()
{return` <a href="#" title="Zoom" id="zoom-button" style="display:none;"> <svg viewBox="0 0 100 100"> <g stroke-width="0" stroke="none"> <polygon points="25,25 5,50 25,75"/> <polygon points="75,25 95,50 75,75"/> </g> <line x1="20" y1="50" x2="80" y2="50" stroke-width="10"></line> </svg> </a> <span id="annotation-label" style="display:none;"></span> `;}
static cssTemplate()
{return TimeSeriesChart.cssTemplate()+` #zoom-button{position:absolute;left:0;top:0;width:1rem;height:1rem;display:block;background:rgba(255,255,255,0.8);-webkit-backdrop-filter:blur(0.3rem);stroke:#666;fill:#666;border:solid 1px #ccc;border-radius:0.2rem;z-index:20}#annotation-label{position:absolute;left:0;top:0;display:inline;background:rgba(255,255,255,0.8);-webkit-backdrop-filter:blur(0.5rem);color:#000;border:solid 1px #ccc;border-radius:0.2rem;padding:0.2rem;font-size:0.8rem;font-weight:normal;line-height:0.9rem;z-index:10;max-width:15rem}`;}
_mouseMove(event)
{var cursorLocation={x:event.offsetX,y:event.offsetY};if(this._startOrContinueDragging(cursorLocation,false)||this._selectionTimeRange)
return;if(this._indicatorIsLocked)
return;var oldIndicatorID=this._indicatorID;var newAnnotation=this._findAnnotation(cursorLocation);var newIndicatorID=null;if(this._currentAnnotation)
newIndicatorID=null;else
newIndicatorID=this._findClosestPoint(cursorLocation);this._forceRender=true;this.enqueueToRender();if(this._currentAnnotation==newAnnotation&&this._indicatorID==newIndicatorID)
return;this._currentAnnotation=newAnnotation;this._indicatorID=newIndicatorID;this._notifyIndicatorChanged();}
_mouseLeave(event)
{if(this._selectionTimeRange||this._indicatorIsLocked||!this._indicatorID)
return;this._indicatorID=null;this._forceRender=true;this.enqueueToRender();this._notifyIndicatorChanged();}
_mouseDown(event)
{this._lastMouseDownLocation={x:event.offsetX,y:event.offsetY};}
_mouseUp(event)
{if(this._dragStarted)
this._endDragging({x:event.offsetX,y:event.offsetY});}
_click(event)
{if(this._selectionTimeRange){if(!this._didEndDrag){this._lastMouseDownLocation=null;this._selectionTimeRange=null;this._notifySelectionChanged(true);this._mouseMove(event);}
return;}
this._lastMouseDownLocation=null;var cursorLocation={x:event.offsetX,y:event.offsetY};var annotation=this._findAnnotation(cursorLocation);if(annotation){this.dispatchAction('annotationClick',annotation);return;}
this._indicatorIsLocked=!this._indicatorIsLocked;this._indicatorID=this._findClosestPoint(cursorLocation);this._forceRender=true;this.enqueueToRender();this._notifyIndicatorChanged();}
_startOrContinueDragging(cursorLocation,didEndDrag)
{var mouseDownLocation=this._lastMouseDownLocation;if(!mouseDownLocation||!this._options.selection)
return false;var xDiff=mouseDownLocation.x-cursorLocation.x;var yDiff=mouseDownLocation.y-cursorLocation.y;if(!this._dragStarted&&xDiff*xDiff+yDiff*yDiff<10)
return false;this._dragStarted=true;var indicatorDidChange=!!this._indicatorID;this._indicatorID=null;this._indicatorIsLocked=false;var metrics=this._layout();var oldSelection=this._selectionTimeRange;if(!didEndDrag){var selectionStart=Math.min(mouseDownLocation.x,cursorLocation.x);var selectionEnd=Math.max(mouseDownLocation.x,cursorLocation.x);this._selectionTimeRange=[metrics.xToTime(selectionStart),metrics.xToTime(selectionEnd)];}
this._forceRender=true;this.enqueueToRender();if(indicatorDidChange)
this._notifyIndicatorChanged();var selectionDidChange=!oldSelection||oldSelection[0]!=this._selectionTimeRange[0]||oldSelection[1]!=this._selectionTimeRange[1];if(selectionDidChange||didEndDrag)
this._notifySelectionChanged(didEndDrag);return true;}
_endDragging(cursorLocation)
{if(!this._startOrContinueDragging(cursorLocation,true))
return;this._dragStarted=false;this._lastMouseDownLocation=null;this._didEndDrag=true;setTimeout(()=>this._didEndDrag=false,0);}
_notifyIndicatorChanged()
{this.dispatchAction('indicatorChange',this._indicatorID,this._indicatorIsLocked);}
_notifySelectionChanged(didEndDrag)
{this.dispatchAction('selectionChange',this._selectionTimeRange,didEndDrag);}
_findAnnotation(cursorLocation)
{if(!this._annotations)
return null;for(const item of this._annotations){if(item.x<=cursorLocation.x&&cursorLocation.x<=item.x+item.width&&item.y<=cursorLocation.y&&cursorLocation.y<=item.y+item.height)
return item;}
return null;}
_findClosestPoint(cursorLocation)
{Instrumentation.startMeasuringTime('InteractiveTimeSeriesChart','findClosestPoint');var metrics=this._layout();function weightedDistance(point){var x=metrics.timeToX(point.time);var y=metrics.valueToY(point.value);var xDiff=cursorLocation.x-x;var yDiff=cursorLocation.y-y;return xDiff*xDiff+yDiff*yDiff/16;}
var minDistance;var minPoint=null;for(var i=0;i<this._sampledTimeSeriesData.length;i++){var series=this._sampledTimeSeriesData[i];var source=this._sourceList[i];if(!series||!source.interactive)
continue;for(let point=series.firstPoint();point;point=series.nextPoint(point)){var distance=weightedDistance(point);if(minDistance===undefined||distance<minDistance){minDistance=distance;minPoint=point;}}}
Instrumentation.endMeasuringTime('InteractiveTimeSeriesChart','findClosestPoint');return minPoint?minPoint.id:null;}
_layout()
{var metrics=super._layout();metrics.doneWork|=this._forceRender;this._forceRender=false;return metrics;}
_sampleTimeSeries(data,maximumNumberOfPoints,excludedPoints)
{if(this._indicatorID)
excludedPoints.add(this._indicatorID);return super._sampleTimeSeries(data,maximumNumberOfPoints,excludedPoints);}
_renderChartContent(context,metrics)
{super._renderChartContent(context,metrics);Instrumentation.startMeasuringTime('InteractiveTimeSeriesChart','renderChartContent');context.lineJoin='miter';if(this._renderedAnnotation!=this._currentAnnotation){this._renderedAnnotation=this._currentAnnotation;var annotation=this._currentAnnotation;if(annotation){var label=annotation.label;var spacing=this._options.annotations.minWidth;this._annotationLabel.textContent=label;if(this._annotationLabel.style.display!='inline')
this._annotationLabel.style.display='inline';var labelWidth=this._annotationLabel.offsetWidth;var labelHeight=this._annotationLabel.offsetHeight;var centerX=annotation.x+annotation.width/2-labelWidth/2;var maxX=metrics.chartX+metrics.chartWidth-labelWidth-2;var x=Math.round(Math.min(maxX,Math.max(metrics.chartX+2,centerX)));var y=Math.floor(annotation.y-labelHeight-1);this._annotationLabel.style.transform=`translate(${x}px, ${y}px)`;}else
this._annotationLabel.style.display='none';}
const indicator=this.currentIndicator();const indicatorOptions=(indicator&&indicator.isLocked?this._options.lockedIndicator:null)||this._options.indicator;if(indicator&&indicatorOptions){context.fillStyle=indicatorOptions.fillStyle||indicatorOptions.lineStyle;context.strokeStyle=indicatorOptions.lineStyle;context.lineWidth=indicatorOptions.lineWidth;const x=metrics.timeToX(indicator.point.time);const y=metrics.valueToY(indicator.point.value);context.beginPath();context.moveTo(x,metrics.chartY);context.lineTo(x,metrics.chartY+metrics.chartHeight);context.stroke();this._fillCircle(context,x,y,indicatorOptions.pointRadius);context.stroke();}
var selectionOptions=this._options.selection;var selectionX2=0;var selectionY2=0;if(this._selectionTimeRange&&selectionOptions){context.fillStyle=selectionOptions.fillStyle;context.strokeStyle=selectionOptions.lineStyle;context.lineWidth=selectionOptions.lineWidth;var x1=metrics.timeToX(this._selectionTimeRange[0]);var x2=metrics.timeToX(this._selectionTimeRange[1]);context.beginPath();selectionX2=x2;selectionY2=metrics.chartHeight-selectionOptions.lineWidth;context.rect(x1,metrics.chartY+selectionOptions.lineWidth/2,x2-x1,metrics.chartHeight-selectionOptions.lineWidth);context.fill();context.stroke();}
if(this._renderedSelection!=selectionX2){this._renderedSelection=selectionX2;if(this._renderedSelection&&this._options.zoomButton&&selectionX2>0&&selectionX2<metrics.chartX+metrics.chartWidth){if(this._zoomButton.style.display)
this._zoomButton.style.display=null;this._zoomButton.style.left=Math.round(selectionX2+metrics.fontSize/4)+'px';this._zoomButton.style.top=Math.floor(selectionY2-metrics.fontSize*1.5-2)+'px';}else
this._zoomButton.style.display='none';}
Instrumentation.endMeasuringTime('InteractiveTimeSeriesChart','renderChartContent');}}
ComponentBase.defineElement('interactive-time-series-chart',InteractiveTimeSeriesChart);class DashboardChartStatusView extends ComponentBase{constructor(metric,chart)
{super('chart-status-view');this._statusEvaluator=new ChartStatusEvaluator(chart,metric);this._renderLazily=new LazilyEvaluatedFunction((status)=>{status=status||{};this.content('current-value').textContent=status.currentValue||'';this.content('comparison').textContent=status.label||'';this.content('comparison').className=status.comparison||'';});}
render()
{this._renderLazily.evaluate(this._statusEvaluator.status());}
static htmlTemplate()
{return`<span id="current-value"></span> <span id="comparison"></span>`;}
static cssTemplate()
{return` :host{display:block}#comparison{padding-left:0.5rem}#comparison.worse{color:#c33}#comparison.better{color:#33c}`;}}
class PaneSelector extends ComponentBase{constructor()
{super('pane-selector');this._currentPlatform=null;this._currentPath=[];this._platformItems=[];this._renderedMetric=null;this._renderedPath=null;this._updateTimer=null;this._platformContainer=this.content().querySelector('#platform');this._testsContainer=this.content().querySelector('#tests');this._callback=null;this._previouslySelectedItem=null;}
render()
{this._renderPlatformList();this._renderTestLists();}
focus()
{var select=this.content().querySelector('select');if(select){if(select.selectedIndex<0)
select.selectedIndex=0;select.focus();}}
_renderPlatformList()
{var currentMetric=null;if(this._currentPath.length){var lastItem=this._currentPath[this._currentPath.length-1];if(lastItem instanceof Metric)
currentMetric=lastItem;}
if(this._renderedMetric!=currentMetric){if(this._platformContainer.firstChild)
this._platformContainer.removeChild(this._platformContainer.firstChild);this._renderedMetric=currentMetric;this._platformItems=[];if(currentMetric){for(var platform of Platform.sortByName(Platform.all())){if(platform.isHidden())
continue;if(platform.hasMetric(currentMetric))
this._platformItems.push(this._createListItem(platform,platform.label()));}
this._platformContainer.appendChild(this._buildList(this._platformItems));}}
for(var li of this._platformItems){if(li.data==this._currentPlatform)
li.selected=true;}}
_renderTestLists()
{if(this._renderedPath==null)
this._replaceList(0,this._buildTestList(Test.topLevelTests()),[]);for(var i=0;i<this._currentPath.length;i++){var item=this._currentPath[i];if(this._renderedPath[i]==item)
continue;if(item instanceof Metric)
break;var newList=this._buildTestList(Test.sortByName(item.childTests()),Metric.sortByName(item.metrics()));this._replaceList(i+1,newList);}
var removeNodeCount=this._testsContainer.childNodes.length-i-1;if(removeNodeCount>0){while(removeNodeCount--)
this._testsContainer.removeChild(this._testsContainer.lastChild);}
for(var i=0;i<this._currentPath.length;i++){var list=this._testsContainer.childNodes[i];var item=this._currentPath[i];for(var j=0;j<list.childNodes.length;j++){var option=list.childNodes[j];if(option.data==item)
option.selected=true;}}
this._renderedPath=this._currentPath;}
_buildTestList(tests,metrics)
{var self=this;var platform=this._currentPlatform;var metricItems=(metrics||[]).map(function(metric){return self._createListItem(metric,metric.label());});var testItems=tests.filter(test=>!test.isHidden()).map(function(test){var data=test;var label=test.label();if(test.onlyContainsSingleMetric()){data=test.metrics()[0];label=test.label()+' ('+data.label()+')';}
return self._createListItem(data,label);});return this._buildList([metricItems,testItems]);}
_replaceList(position,newList)
{var existingList=this._testsContainer.childNodes[position];if(existingList)
this._testsContainer.replaceChild(newList,existingList);else
this._testsContainer.appendChild(newList);}
_createListItem(data,label,hoverCallback,activationCallback)
{var element=ComponentBase.createElement;var item=element('option',{onmousemove:this._selectedItem.bind(this,data),onclick:this._clickedItem.bind(this,data),},label);item.data=data;return item;}
_buildList(items,onchange)
{var self=this;return ComponentBase.createElement('select',{size:10,onmousemove:function(event){},onchange:function(){if(this.selectedIndex>=0)
self._selectedItem(this.options[this.selectedIndex].data);}},items);}
_selectedItem(data)
{if(data==this._previouslySelectedItem)
return;this._previouslySelectedItem=data;if(data instanceof Platform)
this._currentPlatform=data;else{this._currentPath=data.path();if(data instanceof Metric&&data.test().onlyContainsSingleMetric())
this._currentPath.splice(this._currentPath.length-2,1);}
this.enqueueToRender();}
setCallback(callback)
{this._callback=callback;}
_clickedItem(data,event)
{if(!(data instanceof Platform)||!this._callback||!this._currentPlatform||!this._currentPath.length)
return;event.preventDefault();this._callback(this._currentPlatform,this._currentPath[this._currentPath.length-1]);}
static htmlTemplate()
{return` <div class="pane-selector-container"><div id="tests"></div><div id="platform"></div></div> `;}
static cssTemplate()
{return` .pane-selector-container,.pane-selector-container>div{display:flex;flex-direction:row-reverse;height:10rem;font-size:0.9rem;white-space:nowrap}.pane-selector-container select{height:100%;border:solid 1px red;font-size:0.9rem;border:solid 1px #ccc;border-radius:0.2rem;margin-right:0.2rem;background:transparent;max-width:20rem}.pane-selector-container li.selected a{background:rgba(204,153,51,0.1)}`;}}
ComponentBase.defineElement('pane-selector',PaneSelector);class BarGraphGroup{constructor()
{this._bars=[];this._computeRangeLazily=new LazilyEvaluatedFunction(this._computeRange.bind(this));this._showLabels=false;}
addBar(values,valueLabels,mean,interval,barColor,meanIndicatorColor)
{const bar=new BarGraph(this,values,valueLabels,mean,interval,barColor,meanIndicatorColor);this._bars.push({bar,values,interval});return bar;}
showLabels(){return this._showLabels;}
setShowLabels(showLabels)
{this._showLabels=showLabels;for(const entry of this._bars)
entry.bar.enqueueToRender();}
range()
{return this._computeRangeLazily.evaluate(...this._bars);}
_computeRange(...bars)
{Instrumentation.startMeasuringTime('BarGraphGroup','updateGroup');let min=Infinity;let max=-Infinity;for(const entry of bars){for(const value of entry.values){if(isNaN(value))
continue;min=Math.min(min,value);max=Math.max(max,value);}
if(entry.interval){for(const value of entry.interval){if(isNaN(value))
continue;min=Math.min(min,value);max=Math.max(max,value);}}}
const diff=max-min;min-=diff*0.1;max+=diff*0.1;const xForValue=(value,width)=>(value-min)/(max-min)*width;const barRangeForValue=(value,width)=>[0,(value-min)/(max-min)*width];Instrumentation.endMeasuringTime('BarGraphGroup','updateGroup');return{min,max,xForValue,barRangeForValue};}}
class BarGraph extends ComponentBase{constructor(group,values,valueLabels,mean,interval,barColor,meanIndicatorColor)
{super('bar-graph');this._group=group;this._values=values;this._valueLabels=valueLabels;this._mean=mean;this._interval=interval;this._barColor=barColor;this._meanIndicatorColor=meanIndicatorColor;this._resizeCanvasLazily=new LazilyEvaluatedFunction(this._resizeCanvas.bind(this));this._renderCanvasLazily=new LazilyEvaluatedFunction(this._renderCanvas.bind(this));this._renderLabelsLazily=new LazilyEvaluatedFunction(this._renderLabels.bind(this));}
render()
{Instrumentation.startMeasuringTime('SingleBarGraph','render');if(!this._values)
return false;const range=this._group.range();const showLabels=this._group.showLabels();const canvas=this.content('graph');const element=this.element();const width=element.offsetWidth;const height=element.offsetHeight;const scale=this._resizeCanvasLazily.evaluate(canvas,width,height);const step=this._renderCanvasLazily.evaluate(canvas,width,height,scale,this._values,showLabels?null:this._mean,showLabels?null:this._interval,range);this._renderLabelsLazily.evaluate(canvas,step,showLabels?this._valueLabels:null);Instrumentation.endMeasuringTime('SingleBarGraph','render');}
_resizeCanvas(canvas,width,height)
{const scale=window.devicePixelRatio;canvas.width=width*scale;canvas.height=height*scale;canvas.style.width=width+'px';canvas.style.height=height+'px';return scale;}
_renderCanvas(canvas,width,height,scale,values,mean,interval,range)
{const context=canvas.getContext('2d');context.scale(scale,scale);context.clearRect(0,0,width,height);context.fillStyle=this._barColor;context.strokeStyle=this._meanIndicatorColor;context.lineWidth=1;const step=Math.floor(height/values.length);for(let i=0;i<values.length;i++){const value=values[i];if(isNaN(value))
continue;const barWidth=range.xForValue(value,width);const barRange=range.barRangeForValue(value,width);const y=i*step;context.fillRect(0,y,barWidth,step-1);}
const filteredValues=values.filter((value)=>!isNaN(value));if(mean){const x=range.xForValue(mean,width);context.beginPath();context.moveTo(x,0);context.lineTo(x,height);context.stroke();}
if(interval){const x1=range.xForValue(interval[0],width);const x2=range.xForValue(interval[1],width);const errorBarHeight=10;const endBarY1=height/2-errorBarHeight/2;const endBarY2=height/2+errorBarHeight/2;context.beginPath();context.moveTo(x1,endBarY1);context.lineTo(x1,endBarY2);context.moveTo(x1,height/2);context.lineTo(x2,height/2);context.moveTo(x2,endBarY1);context.lineTo(x2,endBarY2);context.stroke();}
return step;}
_renderLabels(canvas,step,valueLabels)
{if(!valueLabels)
valueLabels=[];const element=ComponentBase.createElement;this.renderReplace(this.content('labels'),valueLabels.map((label,i)=>{const labelElement=element('div',{class:'label'},label);labelElement.style.top=(i+0.5)*step+'px';return labelElement;}));canvas.style.opacity=valueLabels.length?0.5:1;}
static htmlTemplate()
{return`<canvas id="graph"></canvas><div id="labels"></div>`;}
static cssTemplate()
{return` :host{display:block!important;overflow:hidden;position:relative}.label{position:absolute;left:0;width:100%;text-align:center;font-size:0.8rem;line-height:0.8rem;margin-top:-0.4rem}`;}}
ComponentBase.defineElement('bar-graph',BarGraph);class ResultsTable extends ComponentBase{constructor(name)
{super(name);this._repositoryList=[];this._analysisResultsView=null;}
setAnalysisResultsView(analysisResultsView)
{console.assert(analysisResultsView instanceof AnalysisResultsView);this._analysisResultsView=analysisResultsView;this.enqueueToRender();}
renderTable(valueFormatter,rowGroups,headingLabel,buildHeaders=(headers)=>headers,buildColumns=(columns,row,rowIndex)=>columns)
{Instrumentation.startMeasuringTime('ResultsTable','renderTable');const[repositoryList,constantCommits]=this._computeRepositoryList(rowGroups);const barGraphGroup=new BarGraphGroup();barGraphGroup.setShowLabels(true);const element=ComponentBase.createElement;let hasGroupHeading=false;const tableBodies=rowGroups.map((group)=>{const groupHeading=group.heading;const revisionSupressionCount={};hasGroupHeading=hasGroupHeading||groupHeading;return element('tbody',group.rows.map((row,rowIndex)=>{const cells=[];if(groupHeading!==undefined&&!rowIndex)
cells.push(element('th',{rowspan:group.rows.length},groupHeading));cells.push(element('td',row.heading()));if(row.labelForWholeRow())
cells.push(element('td',{class:'whole-row-label',colspan:repositoryList.length+1},row.labelForWholeRow()));else{cells.push(element('td',row.resultContent(valueFormatter,barGraphGroup)));cells.push(this._createRevisionListCells(repositoryList,revisionSupressionCount,group,row.commitSet(),rowIndex));}
return element('tr',buildColumns(cells,row,rowIndex));}));});this.renderReplace(this.content().querySelector('table'),[tableBodies.length?element('thead',[buildHeaders([ComponentBase.createElement('th',{colspan:hasGroupHeading?2:1},headingLabel),element('th','Result'),repositoryList.map((repository)=>element('th',repository.label())),]),]):[],tableBodies,]);this.renderReplace(this.content('constant-commits'),constantCommits.map((commit)=>element('li',commit.title())));Instrumentation.endMeasuringTime('ResultsTable','renderTable');}
_createRevisionListCells(repositoryList,revisionSupressionCount,testGroup,commitSet,rowIndex)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;const cells=[];for(const repository of repositoryList){const commit=commitSet?commitSet.commitForRepository(repository):null;if(revisionSupressionCount[repository.id()]){revisionSupressionCount[repository.id()]--;continue;}
let succeedingRowIndex=rowIndex+1;while(succeedingRowIndex<testGroup.rows.length){const succeedingCommitSet=testGroup.rows[succeedingRowIndex].commitSet();if(succeedingCommitSet&&commit!=succeedingCommitSet.commitForRepository(repository))
break;succeedingRowIndex++;}
const rowSpan=succeedingRowIndex-rowIndex;const attributes={class:'revision'};if(rowSpan>1){revisionSupressionCount[repository.id()]=rowSpan-1;attributes['rowspan']=rowSpan;}
if(rowIndex+rowSpan>=testGroup.rows.length)
attributes['class']+=' lastRevision';let content='Missing';if(commit){const url=commit.url();content=url?link(commit.label(),url):commit.label();}
cells.push(element('td',attributes,content));}
return cells;}
_computeRepositoryList(rowGroups)
{const allRepositories=Repository.sortByNamePreferringOnesWithURL(Repository.all());const commitSets=[];for(let group of rowGroups){for(let row of group.rows){const commitSet=row.commitSet();if(commitSet)
commitSets.push(commitSet);}}
if(!commitSets.length)
return[[],[]];const changedRepositorySet=new Set;const constantCommits=new Set;for(let repository of allRepositories){const someCommit=commitSets[0].commitForRepository(repository);if(CommitSet.containsMultipleCommitsForRepository(commitSets,repository))
changedRepositorySet.add(repository);else if(someCommit)
constantCommits.add(someCommit);}
return[allRepositories.filter((repository)=>changedRepositorySet.has(repository)),[...constantCommits]];}
static htmlTemplate()
{return`<table class="results-table"></table><ul id="constant-commits"></ul>`;}
static cssTemplate()
{return` table.results-table{border-collapse:collapse;border:solid 0px #ccc;font-size:0.9rem}.results-table th{text-align:center;font-weight:inherit;font-size:1rem;padding:0.2rem}.results-table td{height:1.4rem;text-align:center}.results-table td.whole-row-label{text-align:left}.results-table thead{color:#c93}.results-table td.revision{vertical-align:top;position:relative}.results-table tr:not(:last-child) td.revision:after{display:block;content:'';position:absolute;left:calc(50% - 0.25rem);bottom:-0.4rem;border-style:solid;border-width:0.25rem;border-color:#999 transparent transparent transparent}.results-table tr:not(:last-child) td.revision:before{display:block;content:'';position:absolute;left:calc(50% - 1px);top:1.2rem;height:calc(100% - 1.4rem);border:solid 1px #999}.results-table tr:not(:last-child) td.revision.lastRevision:after{bottom:-0.2rem}.results-table tr:not(:last-child) td.revision.lastRevision:before{height:calc(100% - 1.6rem)}.results-table tbody:not(:first-child) tr:first-child th,.results-table tbody:not(:first-child) tr:first-child td{border-top:solid 1px #ccc}.results-table bar-graph{display:block;width:7rem;height:1rem}#constant-commits{list-style:none;margin:0;padding:0.5rem 0 0 0.5rem;font-size:0.8rem}#constant-commits:empty{padding:0}#constant-commits li{display:inline}#constant-commits li:not(:last-child):after{content:','}`;}}
class ResultsTableRow{constructor(heading,commitSet)
{this._heading=heading;this._result=null;this._link=null;this._label='-';this._commitSet=commitSet;this._labelForWholeRow=null;}
heading(){return this._heading;}
commitSet(){return this._commitSet;}
setResult(result){this._result=result;}
setLink(link,label)
{this._link=link;this._label=label;}
setLabelForWholeRow(label){this._labelForWholeRow=label;}
labelForWholeRow(){return this._labelForWholeRow;}
resultContent(valueFormatter,barGraphGroup)
{let resultContent=this._label;if(this._result){const value=this._result.value;const interval=this._result.interval;let label=valueFormatter(value);if(interval)
label+=' \u00B1'+((value-interval[0])*100/value).toFixed(2)+'%';resultContent=barGraphGroup.addBar([value],[label],null,null,'#ccc',null);}
return this._link?ComponentBase.createLink(resultContent,this._label,this._link):resultContent;}}
class AnalysisResultsViewer extends ResultsTable{constructor()
{super('analysis-results-viewer');this._startPoint=null;this._endPoint=null;this._metric=null;this._testGroups=null;this._currentTestGroup=null;this._rangeSelectorLabels=[];this._selectedRange={};this._expandedPoints=new Set;this._groupToCellMap=new Map;this._selectorRadioButtonList={};this._renderTestGroupsLazily=new LazilyEvaluatedFunction(this.renderTestGroups.bind(this));}
setRangeSelectorLabels(labels){this._rangeSelectorLabels=labels;}
selectedRange(){return this._selectedRange;}
setPoints(startPoint,endPoint,metric)
{this._metric=metric;this._startPoint=startPoint;this._endPoint=endPoint;this._expandedPoints=new Set;this._expandedPoints.add(startPoint);this._expandedPoints.add(endPoint);this.enqueueToRender();}
setTestGroups(testGroups,currentTestGroup)
{this._testGroups=testGroups;this._currentTestGroup=currentTestGroup;if(currentTestGroup&&this._rangeSelectorLabels.length){const commitSets=currentTestGroup.requestedCommitSets();this._selectedRange={[this._rangeSelectorLabels[0]]:commitSets[0],[this._rangeSelectorLabels[1]]:commitSets[1]};}
this.enqueueToRender();}
setAnalysisResultsView(analysisResultsView)
{console.assert(analysisResultsView instanceof AnalysisResultsView);this._analysisResultsView=analysisResultsView;this.enqueueToRender();}
render()
{super.render();Instrumentation.startMeasuringTime('AnalysisResultsViewer','render');this._renderTestGroupsLazily.evaluate(this._testGroups,this._startPoint,this._endPoint,this._metric,this._analysisResultsView,this._expandedPoints);for(const label of this._rangeSelectorLabels){const commitSet=this._selectedRange[label];if(!commitSet)
continue;const list=this._selectorRadioButtonList[label]||[];for(const item of list){if(item.commitSet.equals(commitSet))
item.radio.checked=true;}}
const selectedCell=this.content().querySelector('td.selected');if(selectedCell)
selectedCell.classList.remove('selected');if(this._groupToCellMap&&this._currentTestGroup){const cell=this._groupToCellMap.get(this._currentTestGroup);if(cell)
cell.classList.add('selected');}
Instrumentation.endMeasuringTime('AnalysisResultsViewer','render');}
renderTestGroups(testGroups,startPoint,endPoint,metric,analysisResults,expandedPoints)
{if(!testGroups||!startPoint||!endPoint||!metric||!analysisResults)
return false;Instrumentation.startMeasuringTime('AnalysisResultsViewer','renderTestGroups');const commitSetsInTestGroups=this._collectCommitSetsInTestGroups(testGroups);const rowToMatchingCommitSets=new Map;const rows=this._buildRowsForPointsAndTestGroups(commitSetsInTestGroups,rowToMatchingCommitSets);const testGroupLayoutMap=new Map;rows.forEach((row,rowIndex)=>{const matchingCommitSets=rowToMatchingCommitSets.get(row);if(!matchingCommitSets){console.assert(row instanceof AnalysisResultsViewer.ExpandableRow);return;}
for(let entry of matchingCommitSets){const testGroup=entry.testGroup();let block=testGroupLayoutMap.get(testGroup);if(!block){block=new AnalysisResultsViewer.TestGroupStackingBlock(testGroup,this._analysisResultsView,this._groupToCellMap,()=>this.dispatchAction('testGroupClick',testGroup));testGroupLayoutMap.set(testGroup,block);}
block.addRowIndex(entry,rowIndex);}});const[additionalColumnsByRow,columnCount]=AnalysisResultsViewer._layoutBlocks(rows.length,testGroups.map((group)=>testGroupLayoutMap.get(group)));for(const label of this._rangeSelectorLabels)
this._selectorRadioButtonList[label]=[];const element=ComponentBase.createElement;const buildHeaders=(headers)=>{return[this._rangeSelectorLabels.map((label)=>element('th',label)),headers,columnCount?element('td',{colspan:columnCount+1,class:'stacking-block'}):[],]};const buildColumns=(columns,row,rowIndex)=>{return[this._rangeSelectorLabels.map((label)=>{if(!row.commitSet())
return element('td','');const radio=element('input',{type:'radio',name:label,onchange:()=>{this._selectedRange[label]=row.commitSet();this.dispatchAction('rangeSelectorClick',label,row);}});this._selectorRadioButtonList[label].push({radio,commitSet:row.commitSet()});return element('td',radio);}),columns,additionalColumnsByRow[rowIndex],];}
this.renderTable(metric.makeFormatter(4),[{rows}],'Point',buildHeaders,buildColumns);Instrumentation.endMeasuringTime('AnalysisResultsViewer','renderTestGroups');return true;}
_collectCommitSetsInTestGroups(testGroups)
{if(!this._testGroups)
return[];var commitSetsInTestGroups=[];for(var group of this._testGroups){var sortedSets=group.requestedCommitSets();for(var i=0;i<sortedSets.length;i++)
commitSetsInTestGroups.push(new AnalysisResultsViewer.CommitSetInTestGroup(group,sortedSets[i],sortedSets[i+1]));}
return commitSetsInTestGroups;}
_buildRowsForPointsAndTestGroups(commitSetsInTestGroups,rowToMatchingCommitSets)
{console.assert(this._startPoint.series==this._endPoint.series);var rowList=[];var pointAfterEnd=this._endPoint.series.nextPoint(this._endPoint);var commitSetsWithPoints=new Set;var pointIndex=0;var previousPoint;for(var point=this._startPoint;point&&point!=pointAfterEnd;point=point.series.nextPoint(point),pointIndex++){const commitSetInPoint=point.commitSet();const matchingCommitSets=[];for(var entry of commitSetsInTestGroups){if(commitSetInPoint.equals(entry.commitSet())&&!commitSetsWithPoints.has(entry)){matchingCommitSets.push(entry);commitSetsWithPoints.add(entry);}}
const hasMatchingTestGroup=!!matchingCommitSets.length;if(!hasMatchingTestGroup&&!this._expandedPoints.has(point))
continue;const row=new ResultsTableRow(pointIndex.toString(),commitSetInPoint);row.setResult(point);if(previousPoint&&previousPoint.series.nextPoint(previousPoint)!=point)
rowList.push(new AnalysisResultsViewer.ExpandableRow(this._expandBetween.bind(this,previousPoint,point)));previousPoint=point;rowToMatchingCommitSets.set(row,matchingCommitSets);rowList.push(row);}
commitSetsInTestGroups.forEach(function(entry){if(commitSetsWithPoints.has(entry))
return;for(var i=0;i<rowList.length;i++){var row=rowList[i];if(!(row instanceof AnalysisResultsViewer.ExpandableRow)&&row.commitSet().equals(entry.commitSet())){rowToMatchingCommitSets.get(row).push(entry);return;}}
var groupTime=entry.commitSet().latestCommitTime();var newRow=new ResultsTableRow(null,entry.commitSet());rowToMatchingCommitSets.set(newRow,[entry]);for(var i=0;i<rowList.length;i++){if(rowList[i]instanceof AnalysisResultsViewer.ExpandableRow)
continue;if(entry.succeedingCommitSet()&&rowList[i].commitSet().equals(entry.succeedingCommitSet())){rowList.splice(i,0,newRow);return;}
var rowTime=rowList[i].commitSet().latestCommitTime();if(rowTime>groupTime){rowList.splice(i,0,newRow);return;}
if(rowTime==groupTime){var repositoriesInNewRow=entry.commitSet().repositories();for(var j=i;j<rowList.length;j++){if(rowList[j]instanceof AnalysisResultsViewer.ExpandableRow)
continue;for(var repository of repositoriesInNewRow){var newCommit=entry.commitSet().commitForRepository(repository);var rowCommit=rowList[j].commitSet().commitForRepository(repository);if(!rowCommit||newCommit.time()<rowCommit.time()){rowList.splice(j,0,newRow);return;}}}}}
var newRow=new ResultsTableRow(null,entry.commitSet());rowToMatchingCommitSets.set(newRow,[entry]);rowList.push(newRow);});return rowList;}
_expandBetween(pointBeforeExpansion,pointAfterExpansion)
{console.assert(pointBeforeExpansion.series==pointAfterExpansion.series);var indexBeforeStart=pointBeforeExpansion.seriesIndex;var indexAfterEnd=pointAfterExpansion.seriesIndex;console.assert(indexBeforeStart+1<indexAfterEnd);var series=pointAfterExpansion.series;var increment=Math.ceil((indexAfterEnd-indexBeforeStart)/5);if(increment<3)
increment=1;const expandedPoints=new Set([...this._expandedPoints]);for(var i=indexBeforeStart+1;i<indexAfterEnd;i+=increment)
expandedPoints.add(series.findPointByIndex(i));this._expandedPoints=expandedPoints;this.enqueueToRender();}
static _layoutBlocks(rowCount,blocks)
{const sortedBlocks=this._sortBlocksByRow(blocks);const columns=[];for(const block of sortedBlocks)
this._insertBlockInFirstAvailableColumn(columns,block);const rows=new Array(rowCount);for(let i=0;i<rowCount;i++)
rows[i]=this._createCellsForRow(columns,i);return[rows,columns.length];}
static _sortBlocksByRow(blocks)
{for(let i=0;i<blocks.length;i++)
blocks[i].index=i;return blocks.slice(0).sort((block1,block2)=>{const startRowDiff=block1.startRowIndex()-block2.startRowIndex();if(startRowDiff)
return startRowDiff;const endRowDiff=block2.endRowIndex()-block1.endRowIndex();if(endRowDiff)
return endRowDiff;return block1.index-block2.index;});}
static _insertBlockInFirstAvailableColumn(columns,newBlock)
{for(const existingColumn of columns){for(let i=0;i<existingColumn.length;i++){const currentBlock=existingColumn[i];if((!i||existingColumn[i-1].endRowIndex()<newBlock.startRowIndex())&&newBlock.endRowIndex()<currentBlock.startRowIndex()){existingColumn.splice(i,0,newBlock);return;}}
const lastBlock=existingColumn[existingColumn.length-1];console.assert(lastBlock);if(lastBlock.endRowIndex()<newBlock.startRowIndex()){existingColumn.push(newBlock);return;}}
columns.push([newBlock]);}
static _createCellsForRow(columns,rowIndex)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;const crateEmptyCell=(rowspan)=>element('td',{rowspan:rowspan,class:'stacking-block'},'');const cells=[element('td',{class:'stacking-block'},'')];for(const blocksInColumn of columns){if(!rowIndex&&blocksInColumn[0].startRowIndex()){cells.push(crateEmptyCell(blocksInColumn[0].startRowIndex()));continue;}
for(let i=0;i<blocksInColumn.length;i++){const block=blocksInColumn[i];if(block.startRowIndex()==rowIndex){cells.push(block.createStackingCell());break;}
const rowCount=i+1<blocksInColumn.length?blocksInColumn[i+1].startRowIndex():this._rowCount;const remainingRows=rowCount-block.endRowIndex()-1;if(rowIndex==block.endRowIndex()+1&&rowIndex<rowCount)
cells.push(crateEmptyCell(remainingRows));}}
return cells;}
static htmlTemplate()
{return`<section class="analysis-view">${ResultsTable.htmlTemplate()}</section>`;}
static cssTemplate()
{return ResultsTable.cssTemplate()+` .analysis-view .stacking-block{position:relative;border:solid 1px #fff;cursor:pointer}.analysis-view .stacking-block a{display:block;text-decoration:none;color:inherit;font-size:0.8rem;padding:0 0.1rem;max-width:3rem}.analysis-view .stacking-block:not(.failed){color:black;opacity:1}.analysis-view .stacking-block.selected,.analysis-view .stacking-block:hover{text-decoration:underline}.analysis-view .stacking-block.selected:before{content:'';position:absolute;left:0px;top:0px;width:calc(100% - 2px);height:calc(100% - 2px);border:solid 1px #333}.analysis-view .stacking-block.failed{background:rgba(128,51,128,0.5)}.analysis-view .stacking-block.unchanged{background:rgba(128,128,128,0.5)}.analysis-view .stacking-block.pending{background:rgba(204,204,51,0.2)}.analysis-view .stacking-block.running{background:rgba(204,204,51,0.5)}.analysis-view .stacking-block.worse{background:rgba(255,102,102,0.5)}.analysis-view .stacking-block.better{background:rgba(102,102,255,0.5)}.analysis-view .point-label-with-expansion-link{font-size:0.7rem}.analysis-view .point-label-with-expansion-link a{color:#999;text-decoration:none}`;}}
ComponentBase.defineElement('analysis-results-viewer',AnalysisResultsViewer);AnalysisResultsViewer.ExpandableRow=class extends ResultsTableRow{constructor(callback)
{super(null,null);this._callback=callback;}
resultContent(){return'';}
heading()
{return ComponentBase.createElement('span',{class:'point-label-with-expansion-link'},[ComponentBase.createLink('(Expand)','Expand',this._callback),]);}}
AnalysisResultsViewer.CommitSetInTestGroup=class{constructor(testGroup,commitSet,succeedingCommitSet)
{console.assert(testGroup instanceof TestGroup);console.assert(commitSet instanceof CommitSet);this._testGroup=testGroup;this._commitSet=commitSet;this._succeedingCommitSet=succeedingCommitSet;}
testGroup(){return this._testGroup;}
commitSet(){return this._commitSet;}
succeedingCommitSet(){return this._succeedingCommitSet;}}
AnalysisResultsViewer.TestGroupStackingBlock=class{constructor(testGroup,analysisResultsView,groupToCellMap,callback)
{this._testGroup=testGroup;this._analysisResultsView=analysisResultsView;this._commitSetIndexRowIndexMap=[];this._groupToCellMap=groupToCellMap;this._callback=callback;}
addRowIndex(commitSetInTestGroup,rowIndex)
{console.assert(commitSetInTestGroup instanceof AnalysisResultsViewer.CommitSetInTestGroup);this._commitSetIndexRowIndexMap.push({commitSet:commitSetInTestGroup.commitSet(),rowIndex});}
testGroup(){return this._testGroup;}
createStackingCell()
{const{label,title,status}=this._computeTestGroupStatus();const cell=ComponentBase.createElement('td',{rowspan:this.endRowIndex()-this.startRowIndex()+1,title,class:'stacking-block '+status,onclick:this._callback,},ComponentBase.createLink(label,title,this._callback));this._groupToCellMap.set(this._testGroup,cell);return cell;}
isComplete(){return this._commitSetIndexRowIndexMap.length>=2;}
startRowIndex(){return this._commitSetIndexRowIndexMap[0].rowIndex;}
endRowIndex(){return this._commitSetIndexRowIndexMap[this._commitSetIndexRowIndexMap.length-1].rowIndex;}
_measurementsForCommitSet(testGroup,commitSet)
{return testGroup.requestsForCommitSet(commitSet).map((request)=>{return this._analysisResultsView.resultForRequest(request);}).filter((result)=>!!result);}
_computeTestGroupStatus()
{if(!this.isComplete())
return{label:null,title:null,status:null};console.assert(this._commitSetIndexRowIndexMap.length<=2);const startValues=this._measurementsForCommitSet(this._testGroup,this._commitSetIndexRowIndexMap[0].commitSet);const endValues=this._measurementsForCommitSet(this._testGroup,this._commitSetIndexRowIndexMap[1].commitSet);const result=this._testGroup.compareTestResults(this._analysisResultsView.metric(),startValues,endValues);return{label:result.label,title:result.fullLabelForMean,status:result.status};}}
class TestGroupResultsViewer extends ComponentBase{constructor()
{super('test-group-results-table');this._analysisResults=null;this._testGroup=null;this._startPoint=null;this._endPoint=null;this._currentMetric=null;this._expandedTests=new Set;this._barGraphCellMap=new Map;this._renderResultsTableLazily=new LazilyEvaluatedFunction(this._renderResultsTable.bind(this));this._renderCurrentMetricsLazily=new LazilyEvaluatedFunction(this._renderCurrentMetrics.bind(this));}
setTestGroup(currentTestGroup)
{this._testGroup=currentTestGroup;this.enqueueToRender();}
setAnalysisResults(analysisResults,metric)
{this._analysisResults=analysisResults;this._currentMetric=metric;if(metric){const path=metric.test().path();for(let i=path.length-2;i>=0;i--)
this._expandedTests.add(path[i]);}
this.enqueueToRender();}
render()
{if(!this._testGroup||!this._analysisResults)
return;this._renderResultsTableLazily.evaluate(this._testGroup,this._expandedTests,...this._analysisResults.topLevelTestsForTestGroup(this._testGroup));this._renderCurrentMetricsLazily.evaluate(this._currentMetric);}
_renderResultsTable(testGroup,expandedTests,...tests)
{let maxDepth=0;for(const test of expandedTests)
maxDepth=Math.max(maxDepth,test.path().length);const element=ComponentBase.createElement;this.renderReplace(this.content('results'),[element('thead',[element('tr',[element('th',{colspan:maxDepth+1},'Test'),element('th',{class:'metric-direction'},''),element('th',{colspan:2},'Results'),element('th','Averages'),element('th','Comparison by mean'),element('th','Comparison by individual iterations')]),]),tests.map((test)=>this._buildRowsForTest(testGroup,expandedTests,test,[],maxDepth,0))]);}
_buildRowsForTest(testGroup,expandedTests,test,sharedPath,maxDepth,depth)
{if(!this._analysisResults.containsTest(test))
return[];const element=ComponentBase.createElement;const rows=element('tbody',test.metrics().map((metric)=>this._buildRowForMetric(testGroup,metric,sharedPath,maxDepth,depth)));if(expandedTests.has(test)){return[rows,test.childTests().map((childTest)=>{return this._buildRowsForTest(testGroup,expandedTests,childTest,test.path(),maxDepth,depth+1);})];}
if(test.childTests().length){const link=ComponentBase.createLink;return[rows,element('tr',{class:'breakdown'},[element('td',{colspan:maxDepth+1},link('(Breakdown)',()=>{this._expandedTests=new Set([...expandedTests,test]);this.enqueueToRender();})),element('td',{colspan:3}),])];}
return rows;}
_buildRowForMetric(testGroup,metric,sharedPath,maxDepth,depth)
{const commitSets=testGroup.requestedCommitSets();const valueMap=this._buildValueMap(testGroup,this._analysisResults.viewForMetric(metric));const formatter=metric.makeFormatter(4);const deltaFormatter=metric.makeFormatter(2,false);const formatValue=(value,interval)=>{const delta=interval?(interval[1]-interval[0])/2:null;let result=value==null||isNaN(value)?'-':formatter(value);if(delta!=null&&!isNaN(delta))
result+=` \u00b1 ${deltaFormatter(delta)}`;return result;}
const barGroup=new BarGraphGroup();const barCells=[];const createConfigurationRow=(commitSet,previousCommitSet,barColor,meanIndicatorColor)=>{const entry=valueMap.get(commitSet);const previousEntry=valueMap.get(previousCommitSet);const comparison=entry&&previousEntry?testGroup.compareTestResults(metric,previousEntry.filteredMeasurements,entry.filteredMeasurements):null;const valueLabels=entry.measurements.map((measurement)=>measurement?formatValue(measurement.value,measurement.interval):'-');const barCell=element('td',{class:'plot-bar'},element('div',barGroup.addBar(entry.allValues,valueLabels,entry.mean,entry.interval,barColor,meanIndicatorColor)));barCell.expandedHeight=+valueLabels.length+'rem';barCells.push(barCell);const significanceForMean=comparison&&comparison.isStatisticallySignificantForMean?'significant':'negligible';const significanceForIndividual=comparison&&comparison.isStatisticallySignificantForIndividual?'significant':'negligible';const changeType=comparison?comparison.changeType:null;return[element('th',testGroup.labelForCommitSet(commitSet)),barCell,element('td',formatValue(entry.mean,entry.interval)),element('td',{class:`comparison ${changeType} ${significanceForMean}`},comparison?comparison.fullLabelForMean:''),element('td',{class:`comparison ${changeType} ${significanceForIndividual}`},comparison?comparison.fullLabelForIndividual:''),];};this._barGraphCellMap.set(metric,{barGroup,barCells});const rowspan=commitSets.length;const element=ComponentBase.createElement;const link=ComponentBase.createLink;const metricName=metric.test().metrics().length==1?metric.test().relativeName(sharedPath):metric.relativeName(sharedPath);const onclick=this.createEventHandler((event)=>{if(this._currentMetric==metric){if(event.target.localName=='bar-graph')
return;this._currentMetric=null;}else
this._currentMetric=metric;this.enqueueToRender();});return[element('tr',{onclick},[this._buildEmptyCells(depth,rowspan),element('th',{colspan:maxDepth-depth+1,rowspan},link(metricName,onclick)),element('td',{class:'metric-direction',rowspan},metric.isSmallerBetter()?'\u21A4':'\u21A6'),createConfigurationRow(commitSets[0],null,'#ddd','#333')]),commitSets.slice(1).map((commitSet,setIndex)=>{return element('tr',{onclick},createConfigurationRow(commitSet,commitSets[setIndex],'#aaa','#000'));})];}
_buildValueMap(testGroup,resultsView)
{const commitSets=testGroup.requestedCommitSets();const map=new Map;for(const commitSet of commitSets){const requests=testGroup.requestsForCommitSet(commitSet);const measurements=requests.map((request)=>resultsView.resultForRequest(request));const filteredMeasurements=measurements.filter((result)=>!!result);const filteredValues=filteredMeasurements.map((measurement)=>measurement.value);const allValues=measurements.map((result)=>result!=null?result.value:NaN);const interval=Statistics.confidenceInterval(filteredValues);map.set(commitSet,{requests,measurements,filteredMeasurements,allValues,mean:Statistics.mean(filteredValues),interval});}
return map;}
_buildEmptyCells(count,rowspan)
{const element=ComponentBase.createElement;const emptyCells=[];for(let i=0;i<count;i++)
emptyCells.push(element('td',{rowspan},''));return emptyCells;}
_renderCurrentMetrics(currentMetric)
{for(const entry of this._barGraphCellMap.values()){for(const cell of entry.barCells){cell.style.height=null;cell.parentNode.className=null;}
entry.barGroup.setShowLabels(false);}
const entry=this._barGraphCellMap.get(currentMetric);if(entry){for(const cell of entry.barCells){cell.style.height=cell.expandedHeight;cell.parentNode.className='selected';}
entry.barGroup.setShowLabels(true);}}
static htmlTemplate()
{return`<table id="results"></table>`;}
static cssTemplate()
{return` table{border-collapse:collapse;margin:0;padding:0}td,th{border:none;padding:0;margin:0;white-space:nowrap}td:not(.metric-direction),th:not(.metric-direction){padding:0.1rem 0.5rem}td:not(.metric-direction){min-width:2rem}td.metric-direction{font-size:large}bar-graph{width:7rem;height:1rem}th{font-weight:inherit}thead th{font-weight:inherit;color:#c93}tr.selected>td,tr.selected>th{background:rgba(204,153,51,0.05)}tr:first-child>td,tr:first-child>th{border-top:solid 1px #eee}tbody th{text-align:left}tbody th,tbody td{cursor:pointer}a{color:inherit;text-decoration:inherit}bar-graph{width:100%;height:100%}td.plot-bar{position:relative;min-width:7rem}td.plot-bar>*{display:block;position:absolute;width:100%;height:100%;top:0;left:0}.comparison{text-align:left}.negligible{color:#999}.significant.worse{color:#c33}.significant.better{color:#33c}tr.breakdown td{padding:0;font-size:small;text-align:center}tr.breakdown a{display:inline-block;text-decoration:none;color:#999;margin-bottom:0.2rem}`;}}
ComponentBase.defineElement('test-group-results-viewer',TestGroupResultsViewer);class TestGroupRevisionTable extends ComponentBase{constructor()
{super('test-group-revision-table');this._testGroup=null;this._analysisResults=null;this._renderTableLazily=new LazilyEvaluatedFunction(this._renderTable.bind(this));}
setTestGroup(testGroup)
{this._testGroup=testGroup;this.enqueueToRender();}
setAnalysisResults(analysisResults)
{this._analysisResults=analysisResults;this.enqueueToRender();}
render()
{this._renderTableLazily.evaluate(this._testGroup,this._analysisResults);}
_renderTable(testGroup,analysisResults)
{if(!testGroup)
return;const commitSets=testGroup.requestedCommitSets();const requestedRepositorySet=new Set;const additionalRepositorySet=new Set;const patchedPepositorySet=new Set;for(const commitSet of commitSets){for(const repository of commitSet.repositories()){requestedRepositorySet.add(repository);if(commitSet.patchForRepository(repository))
patchedPepositorySet.add(repository);}}
const rowEntries=[];let hasCustomRoots=false;let firstRequest=null;commitSets.forEach((commitSet,commitSetIndex)=>{const setLabel=testGroup.labelForCommitSet(commitSet);const buildRequests=testGroup.requestsForCommitSet(commitSet);if(commitSet.customRoots().length)
hasCustomRoots=true;if(!firstRequest)
firstRequest=buildRequests[0];buildRequests.forEach((request,i)=>{const resultCommitSet=analysisResults?analysisResults.commitSetForRequest(request):null;if(resultCommitSet){for(const repository of resultCommitSet.repositories()){if(!requestedRepositorySet.has(repository))
additionalRepositorySet.add(repository);}}
let label=(1+request.order()).toString();if(request.order()<0)
label=`Build ${request.order() - firstRequest.order() + 1}`;else if(firstRequest.isBuild())
label=`Test ${label}`;rowEntries.push({groupHeader:!i?setLabel:null,groupRowCount:buildRequests.length,label,commitSet:resultCommitSet||commitSet,requestedCommitSet:commitSet,customRoots:commitSet.customRoots(),rowCountByRepository:new Map,repositoriesToSkip:new Set,customRootsRowCount:1,request,});});});this._mergeCellsWithSameCommitsAcrossRows(rowEntries);const requestedRepositoryList=Repository.sortByNamePreferringOnesWithURL([...requestedRepositorySet]);const additionalRepositoryList=Repository.sortByNamePreferringOnesWithURL([...additionalRepositorySet]);const element=ComponentBase.createElement;this.renderReplace(this.content('revision-table'),[element('thead',[element('th','Configuration'),requestedRepositoryList.map((repository)=>element('th',repository.name())),hasCustomRoots?element('th','Roots'):[],element('th','Order'),element('th','Status'),additionalRepositoryList.map((repository)=>element('th',repository.name())),]),element('tbody',rowEntries.map((entry)=>{const request=entry.request;return element('tr',[entry.groupHeader?element('td',{rowspan:entry.groupRowCount},entry.groupHeader):[],requestedRepositoryList.map((repository)=>this._buildCommitCell(entry,repository,patchedPepositorySet.has(repository))),hasCustomRoots?this._buildCustomRootsCell(entry):[],element('td',entry.label),this._buildDescriptionCell(request),additionalRepositoryList.map((repository)=>this._buildCommitCell(entry,repository)),]);}))]);}
_buildDescriptionCell(request)
{const link=ComponentBase.createLink;const element=ComponentBase.createElement;const showWarningIcon=request.hasFinished()&&request.statusDescription();const cellContent=[];if(showWarningIcon){const warningIcon=new WarningIcon;warningIcon.setWarning(`Last status: ${request.statusDescription()}`);cellContent.push(element('div',{class:'warning-icon-container'},warningIcon));}
cellContent.push(request.statusUrl()?link(request.statusLabel(),request.statusDescription(),request.statusUrl()):request.statusLabel());const hasInProgressReport=!request.hasFinished()&&request.statusDescription();if(hasInProgressReport)
cellContent.push(element('span',{class:'status-description'},`${request.statusDescription()}`));return element('td',{class:'description-cell'},cellContent);}
_buildCommitCell(entry,repository,showRoot=false)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;if(entry.repositoriesToSkip.has(repository))
return[];const commit=entry.commitSet.commitForRepository(repository);let content=[];if(commit)
content=commit.url()?[link(commit.label(),commit.url())]:[commit.label()];const patch=entry.requestedCommitSet.patchForRepository(repository);if(patch)
content.push(' with ',this._buildFileInfo(patch));if(showRoot){const root=entry.requestedCommitSet.rootForRepository(repository);if(root)
content.push(' (',this._buildFileInfo(root,'Build product'),')');}
return element('td',{rowspan:entry.rowCountByRepository.get(repository)},content);}
_buildCustomRootsCell(entry)
{const rowspan=entry.customRootsRowCount;if(!rowspan)
return[];const element=ComponentBase.createElement;if(!entry.customRoots.length)
return element('td',{class:'roots',rowspan});return element('td',{class:'roots',rowspan},element('ul',entry.customRoots.map((customRoot)=>{return this._buildFileInfo(customRoot);}).map((content)=>element('li',content))));}
_buildFileInfo(file,labelOverride=null)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;let label=labelOverride||file.label();if(file.deletedAt())
return[label,' ',element('span',{class:'purged'},'(Purged)')];return link(label,file.url());}
_mergeCellsWithSameCommitsAcrossRows(rowEntries)
{for(let rowIndex=0;rowIndex<rowEntries.length;rowIndex++){const entry=rowEntries[rowIndex];for(const repository of entry.commitSet.repositories()){if(entry.repositoriesToSkip.has(repository))
continue;const commit=entry.commitSet.commitForRepository(repository);const patch=entry.requestedCommitSet.patchForRepository(repository);let rowCount=1;for(let otherRowIndex=rowIndex+1;otherRowIndex<rowEntries.length;otherRowIndex++){const otherEntry=rowEntries[otherRowIndex];const otherCommit=otherEntry.commitSet.commitForRepository(repository);const otherPatch=otherEntry.requestedCommitSet.patchForRepository(repository);if(commit!=otherCommit||patch!=otherPatch)
break;otherEntry.repositoriesToSkip.add(repository);rowCount++;}
entry.rowCountByRepository.set(repository,rowCount);}
if(entry.customRootsRowCount){let rowCount=1;for(let otherRowIndex=rowIndex+1;otherRowIndex<rowEntries.length;otherRowIndex++){const otherEntry=rowEntries[otherRowIndex];if(!CommitSet.areCustomRootsEqual(entry.customRoots,otherEntry.customRoots))
break;otherEntry.customRootsRowCount=0;rowCount++;}
entry.customRootsRowCount=rowCount;}}}
static htmlTemplate()
{return`<table id="revision-table"></table>`;}
static cssTemplate()
{return` table{border-collapse:collapse}th,td{text-align:center;padding:0.2rem 0.8rem}tbody th,tbody td{border-top:solid 1px #eee;border-bottom:solid 1px #eee}th{font-weight:inherit}.status-description{display:inline-block;max-width:10rem;white-space:nowrap;text-overflow:ellipsis;overflow:hidden}.status-description::before{content:':'}.description-cell{position:relative}div.warning-icon-container{position:absolute;top:-0.2rem;left:0}.roots{max-width:20rem}.purged{color:#999}.roots ul,.roots li{list-style:none;margin:0;padding:0}.roots li{margin-top:0.4rem;margin-bottom:0.4rem}`;}}
ComponentBase.defineElement('test-group-revision-table',TestGroupRevisionTable);class TestGroupForm extends ComponentBase{constructor(name)
{super(name||'test-group-form');this._repetitionCount=4;this._notifyOnCompletion=true;}
setRepetitionCount(count)
{this.content('repetition-count').value=count;this._repetitionCount=count;}
setTestAndPlatform(test,platform)
{this.part('repetition-type-selector').setTestAndPlatform(test,platform);}
updateWithTestGroup(testGroup)
{console.assert(testGroup);this.setRepetitionCount(testGroup.initialRepetitionCount());this.setTestAndPlatform(testGroup.test(),testGroup.platform());this.part('repetition-type-selector').selectedRepetitionType=testGroup.repetitionType();}
didConstructShadowTree()
{const repetitionCountSelect=this.content('repetition-count');repetitionCountSelect.onchange=()=>{this._repetitionCount=repetitionCountSelect.value;};const notifyOnCompletionCheckBox=this.content('notify-on-completion-checkbox');notifyOnCompletionCheckBox.onchange=()=>this._notifyOnCompletion=notifyOnCompletionCheckBox.checked;this.content('form').onsubmit=this.createEventHandler(()=>this.startTesting());}
startTesting()
{const repetitionType=this.part('repetition-type-selector').selectedRepetitionType;this.dispatchAction('startTesting',this._repetitionCount,repetitionType,this._notifyOnCompletion);}
static htmlTemplate()
{return`<form id="form"><button id="start-button" type="submit"><slot>Start A/B testing</slot></button>${this.formContent()}</form>`;}
static cssTemplate()
{return` :host{display:block}#notify-on-completion-checkbox{margin-left:0.5rem;width:1rem}`;}
static formContent()
{return`
            with
            <select id="repetition-count">
                <option>1</option>
                <option>2</option>
                <option>3</option>
                <option selected>4</option>
                <option>5</option>
                <option>6</option>
                <option>7</option>
                <option>8</option>
                <option>9</option>
                <option>10</option>
            </select>
            <label>iterations per set in</label>
            <repetition-type-selection id="repetition-type-selector"></repetition-type-selection>
            <input id="notify-on-completion-checkbox" type="checkbox" checked/>Notify on completion
        `;}}
ComponentBase.defineElement('test-group-form',TestGroupForm);class CustomizableTestGroupForm extends TestGroupForm{constructor()
{super('customizable-test-group-form');this._commitSetMap=null;this._uncustomizedCommitSetMap=null;this._name=null;this._isCustomized=false;this._revisionEditorMap=null;this._ownerRevisionMap=null;this._checkedLabelByPosition=null;this._hasIncompleteOwnedRepository=null;this._fetchingCommitPromises=[];}
setCommitSetMap(map)
{this._commitSetMap=map;this._uncustomizedCommitSetMap=map;this._isCustomized=false;this._fetchingCommitPromises=[];this._checkedLabelByPosition=new Map;this._hasIncompleteOwnedRepository=new Map;this.enqueueToRender();}
startTesting()
{const repetitionType=this.part('repetition-type-selector').selectedRepetitionType;this.dispatchAction('startTesting',this._name,this._repetitionCount,repetitionType,this._computeCommitSetMap(),this._notifyOnCompletion);}
didConstructShadowTree()
{super.didConstructShadowTree();const nameControl=this.content('name');nameControl.oninput=()=>{this._name=nameControl.value;this.enqueueToRender();};this.content('customize-link').onclick=this.createEventHandler(()=>{if(!this._isCustomized){const uncustomizedCommitSetMap=this._commitSetMap;const fetchingCommitPromises=[];this._commitSetMap=new Map;for(const label in uncustomizedCommitSetMap){const intermediateCommitSet=new IntermediateCommitSet(uncustomizedCommitSetMap[label]);fetchingCommitPromises.push(intermediateCommitSet.fetchCommitLogs());this._commitSetMap.set(label,intermediateCommitSet);}
this._fetchingCommitPromises=fetchingCommitPromises;return Promise.all(fetchingCommitPromises).then(()=>{if(this._fetchingCommitPromises!==fetchingCommitPromises)
return;this._isCustomized=true;this._fetchingCommitPromises=[];for(const label in uncustomizedCommitSetMap)
this._checkedLabelByPosition.set(label,new Map);this.enqueueToRender();});}
this._isCustomized=true;this.enqueueToRender();});}
_computeCommitSetMap()
{console.assert(this._commitSetMap);if(!this._isCustomized)
return this._commitSetMap;const map={};for(const[label,commitSet]of this._commitSetMap){const customCommitSet=new CustomCommitSet;for(const repository of commitSet.repositories()){const revisionEditor=this._revisionEditorMap.get(label).get(repository);console.assert(revisionEditor);const ownerRevision=this._ownerRevisionMap.get(label).get(repository)||null;customCommitSet.setRevisionForRepository(repository,revisionEditor.value,null,ownerRevision);}
map[label]=customCommitSet;}
return map;}
render()
{super.render();this.content('start-button').disabled=!(this._commitSetMap&&this._name);this.content('customize-link-container').style.display=!this._commitSetMap?'none':null;this._renderCustomRevisionTable(this._commitSetMap,this._isCustomized,this._uncustomizedCommitSetMap);}
_renderCustomRevisionTable(commitSetMap,isCustomized,uncustomizedCommitSetMap)
{if(!commitSetMap||!isCustomized){this.renderReplace(this.content('custom-table'),[]);return;}
const repositorySet=new Set;const ownedRepositoriesByRepository=new Map;const commitSetLabels=[];this._revisionEditorMap=new Map;this._ownerRevisionMap=new Map;for(const[label,commitSet]of commitSetMap){for(const repository of commitSet.highestLevelRepositories()){repositorySet.add(repository);const ownedRepositories=commitSetMap.get(label).ownedRepositoriesForOwnerRepository(repository);if(!ownedRepositories)
continue;if(!ownedRepositoriesByRepository.has(repository))
ownedRepositoriesByRepository.set(repository,new Set);const ownedRepositorySet=ownedRepositoriesByRepository.get(repository);for(const ownedRepository of ownedRepositories)
ownedRepositorySet.add(ownedRepository)}
commitSetLabels.push(label);this._revisionEditorMap.set(label,new Map);this._ownerRevisionMap.set(label,new Map);}
const repositoryList=Repository.sortByNamePreferringOnesWithURL(Array.from(repositorySet.values()));const element=ComponentBase.createElement;this.renderReplace(this.content('custom-table'),[element('thead',element('tr',[element('td',{colspan:2},'Repository'),commitSetLabels.map((label)=>element('td',{colspan:commitSetLabels.length+1},label)),element('td')])),this._constructTableBodyList(repositoryList,commitSetMap,ownedRepositoriesByRepository,this._hasIncompleteOwnedRepository,uncustomizedCommitSetMap),this._constructTestabilityRows(commitSetMap)]);}
_constructTestabilityRows(commitSetMap)
{const element=ComponentBase.createElement;const commitSets=Array.from(commitSetMap.values());const hasCommitWithTestability=commitSets.some((commitSet)=>!!commitSet.commitsWithTestability().length);if(!hasCommitWithTestability)
return[];const testabilityCells=[];for(const commitSet of commitSetMap.values()){const entries=commitSet.commitsWithTestability().map((commit)=>element('li',`${commit.title()}: ${commit.testability()}`));testabilityCells.push(element('td',{colspan:commitSetMap.size+1,class:'testability'},element('ul',entries)));}
return element('tbody',element('tr',[element('td',{colspan:2}),testabilityCells,element('td')]));}
_constructTableBodyList(repositoryList,commitSetMap,ownedRepositoriesByRepository,hasIncompleteOwnedRepository,uncustomizedCommitSetMap)
{const element=ComponentBase.createElement;const tableBodyList=[];for(const repository of repositoryList){const rows=[];const allCommitSetSpecifiesOwnerCommit=Array.from(commitSetMap.values()).every((commitSet)=>commitSet.ownsCommitsForRepository(repository));const hasIncompleteRow=hasIncompleteOwnedRepository.get(repository);const ownedRepositories=ownedRepositoriesByRepository.get(repository);rows.push(this._constructTableRowForCommitsWithoutOwner(commitSetMap,repository,allCommitSetSpecifiesOwnerCommit,hasIncompleteOwnedRepository,uncustomizedCommitSetMap));if((!ownedRepositories||!ownedRepositories.size)&&!hasIncompleteRow){tableBodyList.push(element('tbody',rows));continue;}
if(ownedRepositories){for(const ownedRepository of ownedRepositories)
rows.push(this._constructTableRowForCommitsWithOwner(commitSetMap,ownedRepository,repository,uncustomizedCommitSetMap));}
if(hasIncompleteRow){const commits=Array.from(commitSetMap.values()).map((commitSet)=>commitSet.commitForRepository(repository));const commitDiff=CommitLog.ownedCommitDifferenceForOwnerCommits(...commits);rows.push(this._constructTableRowForIncompleteOwnedCommits(commitSetMap,repository,commitDiff));}
tableBodyList.push(element('tbody',rows));}
return tableBodyList;}
_constructTableRowForCommitsWithoutOwner(commitSetMap,repository,ownsRepositories,hasIncompleteOwnedRepository,uncustomizedCommitSetMap)
{const element=ComponentBase.createElement;const cells=[element('th',{colspan:2},repository.label())];for(const label of commitSetMap.keys())
cells.push(this._constructRevisionRadioButtons(commitSetMap,repository,label,null,uncustomizedCommitSetMap));if(ownsRepositories){const plusButton=new PlusButton();plusButton.setDisabled(hasIncompleteOwnedRepository.get(repository));plusButton.listenToAction('activate',()=>{this._hasIncompleteOwnedRepository.set(repository,true);this.enqueueToRender();});cells.push(element('td',plusButton));}else
cells.push(element('td'));return element('tr',cells);}
_constructTableRowForCommitsWithOwner(commitSetMap,repository,ownerRepository,uncustomizedCommitSetMap)
{const element=ComponentBase.createElement;const cells=[element('td',{class:'owner-repository-label'}),element('th',repository.label())];const minusButton=new MinusButton();for(const label of commitSetMap.keys())
cells.push(this._constructRevisionRadioButtons(commitSetMap,repository,label,ownerRepository,uncustomizedCommitSetMap));minusButton.listenToAction('activate',()=>{for(const commitSet of commitSetMap.values())
commitSet.removeCommitForRepository(repository)
this.enqueueToRender();});cells.push(element('td',minusButton));return element('tr',cells);}
_constructTableRowForIncompleteOwnedCommits(commitSetMap,ownerRepository,commitDiff)
{const element=ComponentBase.createElement;const configurationCount=commitSetMap.size;const numberOfCellsPerConfiguration=configurationCount+1;const changedRepositories=Array.from(commitDiff.keys());const minusButton=new MinusButton();const comboBox=new ComboBox(changedRepositories.map((repository)=>repository.name()).sort());comboBox.listenToAction('update',(repositoryName)=>{const targetRepository=changedRepositories.find((repository)=>repositoryName===repository.name());const ownedCommitDifferenceForRepository=Array.from(commitDiff.get(targetRepository).values());const commitSetList=Array.from(commitSetMap.values());console.assert(ownedCommitDifferenceForRepository.length===commitSetList.length);for(let i=0;i<commitSetList.length;i++)
commitSetList[i].setCommitForRepository(targetRepository,ownedCommitDifferenceForRepository[i]);this._hasIncompleteOwnedRepository.set(ownerRepository,false);this.enqueueToRender();});const cells=[element('td',{class:'owner-repository-label'}),element('th',comboBox),element('td',{colspan:configurationCount*numberOfCellsPerConfiguration})];minusButton.listenToAction('activate',()=>{this._hasIncompleteOwnedRepository.set(ownerRepository,false);this.enqueueToRender();});cells.push(element('td',minusButton));return element('tr',cells);}
_constructRevisionRadioButtons(commitSetMap,repository,columnLabel,ownerRepository,uncustomizedCommitSetMap)
{const element=ComponentBase.createElement;const commitForColumn=commitSetMap.get(columnLabel).commitForRepository(repository);const revision=commitForColumn?commitForColumn.revision():'';if(commitForColumn&&commitForColumn.ownerCommit())
this._ownerRevisionMap.get(columnLabel).set(repository,commitForColumn.ownerCommit().revision());const revisionEditor=element('input',{disabled:!!ownerRepository,value:revision,onchange:()=>{if(ownerRepository)
return;commitSetMap.get(columnLabel).updateRevisionForOwnerRepository(repository,revisionEditor.value).then(()=>this.enqueueToRender(),()=>{alert(`"${revisionEditor.value}" does not exist in "${repository.name()}".`);revisionEditor.value=revision;});}});this._revisionEditorMap.get(columnLabel).set(repository,revisionEditor);const nodes=[];for(const[labelToChoose,commitSet]of commitSetMap){const commit=commitSet.commitForRepository(repository);const checkedLabel=this._checkedLabelByPosition.get(columnLabel).get(repository)||columnLabel;const checked=labelToChoose==checkedLabel;const radioButton=element('input',{type:'radio',name:`${columnLabel}-${repository.id()}-radio`,checked,onchange:()=>{const uncustomizedCommit=uncustomizedCommitSetMap[labelToChoose].commitForRepository(repository)||commit;commitSetMap.get(columnLabel).setCommitForRepository(repository,uncustomizedCommit);this._checkedLabelByPosition.get(columnLabel).set(repository,labelToChoose);revisionEditor.value=uncustomizedCommit?uncustomizedCommit.revision():'';if(uncustomizedCommit&&uncustomizedCommit.ownerCommit())
this._ownerRevisionMap.get(columnLabel).set(repository,uncustomizedCommit.ownerCommit().revision());this.enqueueToRender();}});nodes.push(element('td',element('label',[radioButton,labelToChoose])));}
nodes.push(element('td',revisionEditor));return nodes;}
static cssTemplate()
{return` #customize-link-container,#customize-link{color:#333}#customize-link-container{margin-left:0.4rem}#custom-table:not(:empty){margin:1rem 0}#custom-table td.owner-repository-label{border-top:solid 2px transparent;border-bottom:solid 1px transparent;min-width:2rem;text-align:right}#custom-table tr:last-child td.owner-repository-label{border-bottom:solid 1px #ddd}#custom-table th{min-width:12rem}#custom-table,#custom-table td,#custom-table th{font-weight:inherit;border-collapse:collapse;border-top:solid 1px #ddd;border-bottom:solid 1px #ddd;padding:0.4rem 0.2rem;font-size:0.9rem}#custom-table thead td,#custom-table th{text-align:center}#notify-on-completion-checkbox{margin-left:0.4rem}#custom-table td.testability{vertical-align:top}#custom-table td.testability ul{text-align:left;color:#c33;max-width:13rem;margin:0 0 0 1rem;padding:0}`;}
static formContent()
{return`
            <input id="name" type="text" placeholder="Test group name">
            ${super.formContent()}
            <span id="customize-link-container">(<a id="customize-link" href="#">Customize</a>)</span>
            <table id="custom-table"></table>
        `;}}
ComponentBase.defineElement('customizable-test-group-form',CustomizableTestGroupForm);class ChartStyles{static resolveConfiguration(platformId,metricId)
{var platform=Platform.findById(platformId);var metric=Metric.findById(metricId);if(!platform||!metric)
return{error:`Invalid platform or metric: ${platformId} and ${metricId}`};var lastModified=platform.lastModified(metric);if(!lastModified)
return{platform:platform,metric:metric,error:`No results on ${platform.label()}`};return{platform:platform,metric:metric,};}
static createSourceList(platform,metric,disableSampling,includeOutlier,showPoint)
{console.assert(platform instanceof Platform);console.assert(metric instanceof Metric);var lastModified=platform.lastModified(metric);console.assert(lastModified);var measurementSet=MeasurementSet.findSet(platform.id(),metric.id(),lastModified);return[this.baselineStyle(measurementSet,disableSampling,includeOutlier,showPoint),this.targetStyle(measurementSet,disableSampling,includeOutlier,showPoint),this.currentStyle(measurementSet,disableSampling,includeOutlier,showPoint),];}
static baselineStyle(measurementSet,disableSampling,includeOutlier,showPoint)
{return{measurementSet:measurementSet,extendToFuture:true,sampleData:!disableSampling,includeOutliers:includeOutlier,type:'baseline',pointStyle:'#f33',pointRadius:showPoint?2:0,lineStyle:showPoint?'#f99':'#f66',lineWidth:1.5,intervalStyle:'rgba(255, 153, 153, 0.25)',intervalWidth:3,foregroundLineStyle:'#f33',foregroundPointRadius:0,backgroundIntervalStyle:'rgba(255, 153, 153, 0.1)',backgroundPointStyle:'#f99',backgroundLineStyle:'#fcc',interactive:true,};}
static targetStyle(measurementSet,disableSampling,includeOutlier,showPoint)
{return{measurementSet:measurementSet,extendToFuture:true,sampleData:!disableSampling,includeOutliers:includeOutlier,type:'target',pointStyle:'#33f',pointRadius:showPoint?2:0,lineStyle:showPoint?'#99f':'#66f',lineWidth:1.5,intervalStyle:'rgba(153, 153, 255, 0.25)',intervalWidth:3,foregroundLineStyle:'#33f',foregroundPointRadius:0,backgroundIntervalStyle:'rgba(153, 153, 255, 0.1)',backgroundPointStyle:'#99f',backgroundLineStyle:'#ccf',};}
static currentStyle(measurementSet,disableSampling,includeOutlier,showPoint)
{return{measurementSet:measurementSet,sampleData:!disableSampling,includeOutliers:includeOutlier,type:'current',pointStyle:'#333',pointRadius:showPoint?2:0,lineStyle:showPoint?'#999':'#666',lineWidth:1.5,intervalStyle:'rgba(153, 153, 153, 0.25)',intervalWidth:3,foregroundLineStyle:'#333',foregroundPointRadius:0,backgroundIntervalStyle:'rgba(153, 153, 153, 0.1)',backgroundPointStyle:'#999',backgroundLineStyle:'#ccc',interactive:true,};}
static dashboardOptions(valueFormatter)
{return{axis:{yAxisWidth:4, xAxisHeight:2, gridStyle:'#ddd',fontSize:0.8, valueFormatter:valueFormatter,},};}
static overviewChartOptions(valueFormatter)
{var options=this.dashboardOptions(valueFormatter);options.axis.yAxisWidth=0; options.selection={lineStyle:'rgba(51, 204, 255, .5)',lineWidth:2,fillStyle:'rgba(51, 204, 255, .125)',}
return options;}
static mainChartOptions(valueFormatter)
{var options=this.dashboardOptions(valueFormatter);options.axis.xAxisEndPadding=5;options.axis.yAxisWidth=5;options.zoomButton=true;options.selection={lineStyle:'#3cf',lineWidth:2,fillStyle:'rgba(51, 204, 255, .125)',}
options.indicator={lineStyle:'#3cf',lineWidth:2,pointRadius:2,};options.lockedIndicator={fillStyle:'#fff',lineStyle:'#36c',lineWidth:2,pointRadius:3,};options.annotations={textStyle:'#000',textBackground:'#fff',minWidth:3,barHeight:7,barSpacing:2};return options;}
static annotationFillStyleForTask(task){if(!task)
return'#888';switch(task.changeType()){case'inconclusive':return'#fcc';case'progression':return'#39f';case'regression':return'#c60';case'unchanged':return'#ccc';}
return'#fc6';}}
class ChartStatusEvaluator{constructor(chart,metric)
{this._chart=chart;this._computeStatus=new LazilyEvaluatedFunction((currentPoint,previousPoint,view)=>{if(!currentPoint)
return null;const baselineView=this._chart.sampledTimeSeriesData('baseline');const targetView=this._chart.sampledTimeSeriesData('target');return ChartStatusEvaluator.computeChartStatus(metric,currentPoint,previousPoint,view,baselineView,targetView);});}
status()
{const referencePoints=this._chart.referencePoints('current');const currentPoint=referencePoints?referencePoints.currentPoint:null;const previousPoint=referencePoints?referencePoints.previousPoint:null;const view=referencePoints?referencePoints.view:null;return this._computeStatus.evaluate(currentPoint,previousPoint,view);}
static computeChartStatus(metric,currentPoint,previousPoint,currentView,baselineView,targetView)
{const formatter=metric.makeFormatter(3);const deltaFormatter=metric.makeFormatter(2,true);const smallerIsBetter=metric.isSmallerBetter();const labelForDiff=(diff,referencePoint,name,comparison)=>{const relativeDiff=Math.abs(diff*100).toFixed(1);const referenceValue=referencePoint?` (${formatter(referencePoint.value)})`:'';if(comparison!='until')
comparison+=' than';return`${relativeDiff}% ${comparison} ${name}${referenceValue}`;}
const pointIsInCurrentSeries=baselineView!=currentView&&targetView!=currentView;const baselinePoint=pointIsInCurrentSeries&&baselineView?baselineView.lastPointInTimeRange(0,currentPoint.time):null;const targetPoint=pointIsInCurrentSeries&&targetView?targetView.lastPointInTimeRange(0,currentPoint.time):null;const diffFromBaseline=baselinePoint?(currentPoint.value-baselinePoint.value)/baselinePoint.value:undefined;const diffFromTarget=targetPoint?(currentPoint.value-targetPoint.value)/targetPoint.value:undefined;let label=null;let comparison=null;if(diffFromBaseline!==undefined&&diffFromTarget!==undefined){if(diffFromBaseline>0==smallerIsBetter){comparison='worse';label=labelForDiff(diffFromBaseline,baselinePoint,'baseline',comparison);}else if(diffFromTarget<0==smallerIsBetter){comparison='better';label=labelForDiff(diffFromTarget,targetPoint,'target',comparison);}else
label=labelForDiff(diffFromTarget,targetPoint,'target','until');}else if(diffFromBaseline!==undefined){comparison=diffFromBaseline>0==smallerIsBetter?'worse':'better';label=labelForDiff(diffFromBaseline,baselinePoint,'baseline',comparison);}else if(diffFromTarget!==undefined){comparison=diffFromTarget<0==smallerIsBetter?'better':'worse';label=labelForDiff(diffFromTarget,targetPoint,'target','until');}
let valueDelta=null;let relativeDelta=null;if(previousPoint){valueDelta=deltaFormatter(currentPoint.value-previousPoint.value);relativeDelta=(currentPoint.value-previousPoint.value)/previousPoint.value;relativeDelta=(relativeDelta*100).toFixed(0)+'%';}
return{comparison,label,currentValue:formatter(currentPoint.value),valueDelta,relativeDelta};}}
class ChartRevisionRange{constructor(chart,metric)
{this._chart=chart;const thisClass=new.target;this._computeRevisionList=new LazilyEvaluatedFunction((currentPoint,prevoiusPoint)=>{return thisClass._computeRevisionList(currentPoint,prevoiusPoint);});this._computeRevisionRange=new LazilyEvaluatedFunction((repository,currentPoint,previousPoint)=>{return{repository,from:thisClass._revisionForPoint(repository,previousPoint),to:thisClass._revisionForPoint(repository,currentPoint)};});}
revisionList()
{const referencePoints=this._chart.referencePoints('current');const currentPoint=referencePoints?referencePoints.currentPoint:null;const previousPoint=referencePoints?referencePoints.previousPoint:null;return this._computeRevisionList.evaluate(currentPoint,previousPoint);}
rangeForRepository(repository)
{const referencePoints=this._chart.referencePoints('current');const currentPoint=referencePoints?referencePoints.currentPoint:null;const previousPoint=referencePoints?referencePoints.previousPoint:null;return this._computeRevisionRange.evaluate(repository,currentPoint,previousPoint);}
static _revisionForPoint(repository,point)
{if(!point||!repository)
return null;const commitSet=point.commitSet();if(!commitSet)
return null;const commit=commitSet.commitForRepository(repository);if(!commit)
return null;return commit.revision();}
static _computeRevisionList(currentPoint,previousPoint)
{if(!currentPoint)
return null;const currentCommitSet=currentPoint.commitSet();const previousCommitSet=previousPoint?previousPoint.commitSet():null;const repositoriesInCurrentCommitSet=Repository.sortByNamePreferringOnesWithURL(currentCommitSet.repositories());const revisionList=[];for(let repository of repositoriesInCurrentCommitSet){let currentCommit=currentCommitSet.commitForRepository(repository);let previousCommit=previousCommitSet?previousCommitSet.commitForRepository(repository):null;revisionList.push(currentCommit.diff(previousCommit));}
return revisionList;}}
class ChartPaneBase extends ComponentBase{constructor(name)
{super(name);this._errorMessage=null;this._platformId=null;this._metricId=null;this._platform=null;this._metric=null;this._disableSampling=false;this._showOutliers=false;this._openRepository=null;this._overviewChart=null;this._mainChart=null;this._mainChartStatus=null;this._commitLogViewer=null;this._tasksForAnnotations=null;this._detectedAnnotations=null;this._renderAnnotationsLazily=new LazilyEvaluatedFunction(this._renderAnnotations.bind(this));}
configure(platformId,metricId)
{var result=ChartStyles.resolveConfiguration(platformId,metricId);this._errorMessage=result.error;this._platformId=platformId;this._metricId=metricId;this._platform=result.platform;this._metric=result.metric;this._overviewChart=null;this._mainChart=null;this._mainChartStatus=null;this._commitLogViewer=this.content().querySelector('commit-log-viewer').component();if(result.error)
return;var formatter=result.metric.makeFormatter(4);this._overviewChart=new InteractiveTimeSeriesChart(this._createSourceList(false),ChartStyles.overviewChartOptions(formatter));this._overviewChart.listenToAction('selectionChange',this._overviewSelectionDidChange.bind(this));this.renderReplace(this.content().querySelector('.chart-pane-overview'),this._overviewChart);this._mainChart=new InteractiveTimeSeriesChart(this._createSourceList(true),ChartStyles.mainChartOptions(formatter));this._mainChart.listenToAction('dataChange',()=>this._didFetchData());this._mainChart.listenToAction('indicatorChange',this._indicatorDidChange.bind(this));this._mainChart.listenToAction('selectionChange',this._mainSelectionDidChange.bind(this));this._mainChart.listenToAction('zoom',this._mainSelectionDidZoom.bind(this));this._mainChart.listenToAction('annotationClick',this._didClickAnnotation.bind(this));this.renderReplace(this.content().querySelector('.chart-pane-main'),this._mainChart);this._revisionRange=new ChartRevisionRange(this._mainChart);this._mainChartStatus=new ChartPaneStatusView(result.metric,this._mainChart);this._mainChartStatus.setCurrentRepository(this._openRepository);this._mainChartStatus.listenToAction('openRepository',this.openNewRepository.bind(this));this.renderReplace(this.content().querySelector('.chart-pane-details'),this._mainChartStatus);this.content().querySelector('.chart-pane').addEventListener('keydown',this._keyup.bind(this));this.fetchAnalysisTasks(false);}
isSamplingEnabled(){return!this._disableSampling;}
setSamplingEnabled(enabled)
{this._disableSampling=!enabled;this._updateSourceList();}
isShowingOutliers(){return this._showOutliers;}
setShowOutliers(show)
{this._showOutliers=!!show;this._updateSourceList();}
_createSourceList(isMainChart)
{return ChartStyles.createSourceList(this._platform,this._metric,this._disableSampling,this._showOutliers,isMainChart);}
_updateSourceList()
{this._mainChart.setSourceList(this._createSourceList(true));this._overviewChart.setSourceList(this._createSourceList(false));}
fetchAnalysisTasks(noCache)
{var self=this;AnalysisTask.fetchByPlatformAndMetric(this._platformId,this._metricId,noCache).then(function(tasks){self._tasksForAnnotations=tasks;self.enqueueToRender();});}
didUpdateAnnotations()
{this._tasksForAnnotations=[...this._tasksForAnnotations];this.enqueueToRender();}
platformId(){return this._platformId;}
metricId(){return this._metricId;}
setOverviewDomain(startTime,endTime)
{if(this._overviewChart)
this._overviewChart.setDomain(startTime,endTime);}
setMainDomain(startTime,endTime)
{if(this._mainChart)
this._mainChart.setDomain(startTime,endTime);}
setMainSelection(selection)
{if(this._mainChart)
this._mainChart.setSelection(selection);}
setOpenRepository(repository)
{this._openRepository=repository;if(this._mainChartStatus)
this._mainChartStatus.setCurrentRepository(repository);this._updateCommitLogViewer();}
_overviewSelectionDidChange(domain,didEndDrag){}
_mainSelectionDidChange(selection,didEndDrag)
{this._updateCommitLogViewer();}
_mainSelectionDidZoom(selection)
{this._overviewChart.setSelection(selection,this);this._mainChart.setSelection(null);this.enqueueToRender();}
_indicatorDidChange(indicatorID,isLocked)
{this._updateCommitLogViewer();}
_didFetchData()
{this._updateCommitLogViewer();}
_updateCommitLogViewer()
{if(!this._revisionRange)
return;const range=this._revisionRange.rangeForRepository(this._openRepository);this._commitLogViewer.view(this._openRepository,range.from,range.to);this.enqueueToRender();}
_didClickAnnotation(annotation)
{if(annotation.task)
this._openAnalysisTask(annotation);else{const newSelection=[annotation.startTime,annotation.endTime];this._mainChart.setSelection(newSelection);this._overviewChart.setSelection(newSelection,this);this.enqueueToRender();}}
_openAnalysisTask(annotation)
{var router=this.router();console.assert(router);window.open(router.url(`analysis/task/${annotation.task.id()}`),'_blank');}
router(){return null;}
openNewRepository(repository)
{this.content().querySelector('.chart-pane').focus();this.setOpenRepository(repository);}
_keyup(event)
{switch(event.keyCode){case 37: if(!this._mainChart.moveLockedIndicatorWithNotification(false))
return;break;case 39: if(!this._mainChart.moveLockedIndicatorWithNotification(true))
return;break;case 38: if(!this._moveOpenRepository(false))
return;break;case 40: if(!this._moveOpenRepository(true))
return;break;default:return;}
this.enqueueToRender();event.preventDefault();event.stopPropagation();}
_moveOpenRepository(forward)
{const openRepository=this._openRepository;if(!openRepository)
return false;const revisionList=this._revisionRange.revisionList();if(!revisionList)
return false;const currentIndex=revisionList.findIndex((info)=>info.repository==openRepository);console.assert(currentIndex>=0);const newIndex=currentIndex+(forward?1:-1);if(newIndex<0||newIndex>=revisionList.length)
return false;this.openNewRepository(revisionList[newIndex].repository);return true;}
render()
{Instrumentation.startMeasuringTime('ChartPane','render');super.render();if(this._overviewChart)
this._overviewChart.enqueueToRender();if(this._mainChart){this._mainChart.enqueueToRender();this._renderAnnotationsLazily.evaluate(this._tasksForAnnotations,this._detectedAnnotations);}
if(this._errorMessage){this.renderReplace(this.content().querySelector('.chart-pane-main'),this._errorMessage);return;}
if(this._mainChartStatus)
this._mainChartStatus.enqueueToRender();var body=this.content().querySelector('.chart-pane-body');if(this._openRepository)
body.classList.add('has-second-sidebar');else
body.classList.remove('has-second-sidebar');Instrumentation.endMeasuringTime('ChartPane','render');}
_renderAnnotations(taskForAnnotations,detectedAnnotations)
{let annotations=(taskForAnnotations||[]).map((task)=>{return{task,fillStyle:ChartStyles.annotationFillStyleForTask(task),startTime:task.startTime(),endTime:task.endTime(),label:task.label()};});annotations=annotations.concat(detectedAnnotations||[]);this._mainChart.setAnnotations(annotations);}
static htmlTemplate()
{return` <section class="chart-pane" tabindex="0"> ${this.paneHeaderTemplate()} <div class="chart-pane-body"> <div class="chart-pane-main"></div> <div class="chart-pane-sidebar"> <div class="chart-pane-overview"></div> <div class="chart-pane-details"></div> </div> <div class="chart-pane-second-sidebar"> <commit-log-viewer></commit-log-viewer> </div> </div> </section> ${this.paneFooterTemplate()} `;}
static paneHeaderTemplate(){return'';}
static paneFooterTemplate(){return'';}
static cssTemplate()
{return Toolbar.cssTemplate()+` .chart-pane { padding: 0rem; height: 18rem; outline: none; } .chart-pane:focus .chart-pane-header { background: rgba(204, 153, 51, 0.1); } .chart-pane-body { position: relative; width: 100%; height: 100%; } .chart-pane-main { padding-right: 20rem; height: 100%; margin: 0; vertical-align: middle; text-align: center; } .has-second-sidebar .chart-pane-main { padding-right: 40rem; } .chart-pane-main > * { width: 100%; height: 100%; } .chart-pane-sidebar, .chart-pane-second-sidebar { position: absolute; right: 0; top: 0; width: 0; border-left: solid 1px #ccc; height: 100%; } :not(.has-second-sidebar) > .chart-pane-second-sidebar { border-left: 0; } .chart-pane-sidebar { width: 20rem; } .has-second-sidebar .chart-pane-sidebar { right: 20rem; } .has-second-sidebar .chart-pane-second-sidebar { width: 20rem; } .chart-pane-overview { width: 100%; height: 5rem; border-bottom: solid 1px #ccc; } .chart-pane-overview > * { display: block; width: 100%; height: 100%; } .chart-pane-details { position: relative; display: block; height: calc(100% - 5.5rem - 2px); overflow-y: scroll; padding-top: 0.5rem; } `;}}
class MutableListView extends ComponentBase{constructor()
{super('mutable-list-view');this._list=[];this._kindList=[];this._kindMap=new Map;this.content().querySelector('form').onsubmit=this._submitted.bind(this);}
setList(list)
{this._list=list;this.enqueueToRender();}
setKindList(list)
{this._kindList=list;this.enqueueToRender();}
render()
{this.renderReplace(this.content().querySelector('.mutable-list'),this._list.map(function(item){console.assert(item instanceof MutableListItem);return item.content();}));var element=ComponentBase.createElement;var kindMap=this._kindMap;kindMap.clear();this.renderReplace(this.content().querySelector('.kind'),this._kindList.map(function(kind){kindMap.set(kind.id(),kind);return element('option',{value:kind.id()},kind.label());}));}
_submitted(event)
{event.preventDefault();const kind=this._kindMap.get(this.content().querySelector('.kind').value);const item=this.content().querySelector('.value').value;this.dispatchAction('addItem',kind,item);}
static cssTemplate()
{return` .mutable-list,.mutable-list li{list-style:none;padding:0;margin:0}.mutable-list:not(:empty){margin-bottom:1rem}.mutable-list{margin-bottom:1rem}.new-list-item-form{white-space:nowrap}`;}
static htmlTemplate()
{return` <ul class="mutable-list"></ul> <form class="new-list-item-form"> <select class="kind"></select> <input class="value"> <button type="submit">Add</button> </form>`;}}
class MutableListItem{constructor(kind,value,valueTitle,valueLink,removalTitle,removalLink)
{this._kind=kind;this._value=value;this._valueTitle=valueTitle;this._valueLink=valueLink;this._removalTitle=removalTitle;this._removalLink=removalLink;}
content()
{const link=ComponentBase.createLink;const closeButton=new CloseButton;closeButton.listenToAction('activate',this._removalLink);return ComponentBase.createElement('li',[this._kind.label(),' ',link(this._value,this._valueTitle,this._valueLink),' ',link(closeButton,this._removalTitle,this._removalLink)]);}}
ComponentBase.defineElement('mutable-list-view',MutableListView);class AnalysisTaskBugList extends ComponentBase{constructor()
{super('analysis-task-bug-list');this._task=null;}
setTask(task)
{console.assert(task==null||task instanceof AnalysisTask);this._task=task;this.enqueueToRender();}
didConstructShadowTree()
{this.part('bug-list').setKindList(BugTracker.all());this.part('bug-list').listenToAction('addItem',(tracker,bugNumber)=>this._associateBug(tracker,bugNumber));}
render()
{const bugList=this._task?this._task.bugs().map((bug)=>{return new MutableListItem(bug.bugTracker(),bug.label(),bug.title(),bug.url(),'Dissociate this bug',()=>this._dissociateBug(bug));}):[];this.part('bug-list').setList(bugList);}
_associateBug(tracker,bugNumber)
{console.assert(tracker instanceof BugTracker);bugNumber=parseInt(bugNumber);return this._task.associateBug(tracker,bugNumber).then(()=>this.enqueueToRender(),(error)=>{this.enqueueToRender();alert('Failed to associate the bug: '+error);});}
_dissociateBug(bug)
{return this._task.dissociateBug(bug).then(()=>this.enqueueToRender(),(error)=>{this.enqueueToRender();alert('Failed to dissociate the bug: '+error);});}
static htmlTemplate(){return`<mutable-list-view id="bug-list"></mutable-list-view>`;}}
ComponentBase.defineElement('analysis-task-bug-list',AnalysisTaskBugList);class RatioBarGraph extends ComponentBase{constructor()
{super('ratio-bar-graph');this._ratio=null;this._label=null;this._shouldRender=true;this._ratioBarGraph=this.content().querySelector('.ratio-bar-graph');}
update(ratio,label,showWarningIcon)
{console.assert(typeof(ratio)=='number');this._ratio=ratio;this._label=label;this._showWarningIcon=showWarningIcon;this._shouldRender=true;}
render()
{if(!this._shouldRender)
return;this._shouldRender=false;var element=ComponentBase.createElement;var children=[element('div',{class:'separator'})];if(this._showWarningIcon){if(this._ratio&&this._ratio<-0.4)
this._ratioBarGraph.classList.add('warning-on-right');else
this._ratioBarGraph.classList.remove('warning-on-right');children.push(new WarningIcon);}
var barClassName='bar';var labelClassName='label';if(this._ratio){var ratioType=this._ratio>0?'better':'worse';barClassName=[barClassName,ratioType].join(' ');labelClassName=[labelClassName,ratioType].join(' ');children.push(element('div',{class:barClassName,style:'width:'+Math.min(Math.abs(this._ratio*100),50)+'%'}));}
if(this._label)
children.push(element('div',{class:labelClassName},this._label));this.renderReplace(this._ratioBarGraph,children);}
static htmlTemplate()
{return`<div class="ratio-bar-graph"></div>`;}
static cssTemplate()
{return` .ratio-bar-graph{position:relative;display:block;margin-left:auto;margin-right:auto;width:10rem;height:2.5rem;overflow:hidden;text-decoration:none;color:black}.ratio-bar-graph warning-icon{position:absolute;display:block;top:0}.ratio-bar-graph:not(.warning-on-right) warning-icon{left:0}.ratio-bar-graph.warning-on-right warning-icon{transform:scaleX(-1);right:0}.ratio-bar-graph .separator{position:absolute;left:50%;width:0px;height:100%;border-left:solid 1px #ccc}.ratio-bar-graph .bar{position:absolute;left:50%;top:0.5rem;height:calc(100% - 1rem);background:#ccc}.ratio-bar-graph .bar.worse{transform:translateX(-100%);background:#c33}.ratio-bar-graph .bar.better{background:#3c3}.ratio-bar-graph .label{position:absolute;line-height:2.5rem}.ratio-bar-graph .label.worse{text-align:left;left:calc(50% + 0.2rem)}.ratio-bar-graph .label.better{text-align:right;right:calc(50% + 0.2rem)}`;}}class CustomAnalysisTaskConfigurator extends ComponentBase{constructor()
{super('custom-analysis-task-configurator');this._selectedTests=[];this._triggerablePlatforms=[];this._selectedPlatform=null;this._showComparison=false;this._commitSetMap={};this._specifiedRevisions={'Baseline':new Map,'Comparison':new Map};this._patchUploaders={'Baseline':new Map,'Comparison':new Map};this._customRootUploaders={'Baseline':null,'Comparison':null};this._fetchedCommits={'Baseline':new Map,'Comparison':new Map};this._repositoryGroupByConfiguration={'Baseline':null,'Comparison':null};this._invalidRevisionsByConfiguration={'Baseline':new Map,'Comparison':new Map};this._updateTriggerableLazily=new LazilyEvaluatedFunction(this._updateTriggerable.bind(this));this._renderTriggerableTestsLazily=new LazilyEvaluatedFunction(this._renderTriggerableTests.bind(this));this._renderTriggerablePlatformsLazily=new LazilyEvaluatedFunction(this._renderTriggerablePlatforms.bind(this));this._renderRepositoryPanesLazily=new LazilyEvaluatedFunction(this._renderRepositoryPanes.bind(this));}
tests(){return this._selectedTests;}
platform(){return this._selectedPlatform;}
commitSets()
{const map=this._commitSetMap;if(!map['Baseline']||!map['Comparison'])
return null;return[map['Baseline'],map['Comparison']];}
selectTests(selectedTests)
{this._selectedTests=selectedTests;this._triggerablePlatforms=Triggerable.triggerablePlatformsForTests(this._selectedTests);if(this._selectedTests.length&&!this._triggerablePlatforms.includes(this._selectedPlatform))
this._selectedPlatform=null;this._didUpdateSelectedPlatforms();}
selectPlatform(selectedPlatform)
{this._selectedPlatform=selectedPlatform;const[triggerable,error]=this._updateTriggerableLazily.evaluate(this._selectedTests,this._selectedPlatform);this._updateRepositoryGroups(triggerable);this._didUpdateSelectedPlatforms();}
_didUpdateSelectedPlatforms()
{for(const configuration of['Baseline','Comparison']){this._updateMapFromSpecifiedRevisionsForConfiguration(this._fetchedCommits,configuration);this._updateMapFromSpecifiedRevisionsForConfiguration(this._invalidRevisionsByConfiguration,configuration);}
this._updateCommitSetMap();this.dispatchAction('testConfigChange');this.enqueueToRender();}
_updateMapFromSpecifiedRevisionsForConfiguration(map,configuration)
{const referenceMap=this._specifiedRevisions[configuration];const newValue=new Map;for(const[key,value]of map[configuration].entries()){if(!referenceMap.has(key))
continue;newValue.set(key,value);}
if(newValue.size!==map[configuration].size)
map[configuration]=newValue;}
setCommitSets(baselineCommitSet,comparisonCommitSet)
{const[triggerable,error]=this._updateTriggerableLazily.evaluate(this._selectedTests,this._selectedPlatform);if(!triggerable)
return;const baselineRepositoryGroup=triggerable.repositoryGroups().find((repositoryGroup)=>repositoryGroup.accepts(baselineCommitSet));if(baselineRepositoryGroup){this._repositoryGroupByConfiguration['Baseline']=baselineRepositoryGroup;this._setUploadedFilesToUploader(this._customRootUploaders['Baseline'],baselineCommitSet.customRoots());this._specifiedRevisions['Baseline']=this._revisionMapFromCommitSet(baselineCommitSet);this._setPatchFiles('Baseline',baselineCommitSet);}
const comparisonRepositoryGroup=triggerable.repositoryGroups().find((repositoryGroup)=>repositoryGroup.accepts(baselineCommitSet));if(comparisonRepositoryGroup){this._repositoryGroupByConfiguration['Comparison']=comparisonRepositoryGroup;this._setUploadedFilesToUploader(this._customRootUploaders['Comparison'],comparisonCommitSet.customRoots());this._specifiedRevisions['Comparison']=this._revisionMapFromCommitSet(comparisonCommitSet);this._setPatchFiles('Comparison',comparisonCommitSet);}
this._showComparison=true;this._updateCommitSetMap();}
_setUploadedFilesToUploader(uploader,files)
{if(!uploader||uploader.hasFileToUpload()||uploader.uploadedFiles().length)
return;uploader.clearUploads();for(const uploadedFile of files)
uploader.addUploadedFile(uploadedFile);}
_setPatchFiles(configurationName,commitSet)
{for(const repository of commitSet.repositories()){const patch=commitSet.patchForRepository(repository);if(patch)
this._setUploadedFilesToUploader(this._ensurePatchUploader(configurationName,repository),[patch]);}}
_revisionMapFromCommitSet(commitSet)
{const revisionMap=new Map;for(const repository of commitSet.repositories())
revisionMap.set(repository,commitSet.revisionForRepository(repository));return revisionMap;}
didConstructShadowTree()
{this.content('specify-comparison-button').onclick=this.createEventHandler(()=>this._configureComparison());const createRootUploader=()=>{const uploader=new InstantFileUploader;uploader.allowMultipleFiles();uploader.element().textContent='Add a new root';uploader.listenToAction('removedFile',()=>this._updateCommitSetMap());return uploader;}
const baselineRootsUploader=createRootUploader();baselineRootsUploader.listenToAction('uploadedFile',(uploadedFile)=>this._updateCommitSetMap());this._customRootUploaders['Baseline']=baselineRootsUploader;const comparisonRootsUploader=createRootUploader();comparisonRootsUploader.listenToAction('uploadedFile',()=>this._updateCommitSetMap());this._customRootUploaders['Comparison']=comparisonRootsUploader;}
_ensurePatchUploader(configurationName,repository)
{const uploaderMap=this._patchUploaders[configurationName];let uploader=uploaderMap.get(repository);if(uploader)
return uploader;uploader=new InstantFileUploader;uploader.element().textContent='Apply a patch';uploader.listenToAction('uploadedFile',()=>this._updateCommitSetMap());uploader.listenToAction('removedFile',()=>this._updateCommitSetMap());uploaderMap.set(repository,uploader);return uploader;}
_configureComparison()
{this._showComparison=true;this._repositoryGroupByConfiguration['Comparison']=this._repositoryGroupByConfiguration['Baseline'];const specifiedBaselineRevisions=this._specifiedRevisions['Baseline'];const specifiedComparisonRevisions=new Map;for(let key of specifiedBaselineRevisions.keys())
specifiedComparisonRevisions.set(key,specifiedBaselineRevisions.get(key));this._specifiedRevisions['Comparison']=specifiedComparisonRevisions;for(const[repository,patchUploader]of this._patchUploaders['Baseline']){const files=patchUploader.uploadedFiles();if(!files.length)
continue;const comparisonPatchUploader=this._ensurePatchUploader('Comparison',repository);for(const uploadedFile of files)
comparisonPatchUploader.addUploadedFile(uploadedFile);}
const comparisonRootUploader=this._customRootUploaders['Comparison'];for(const uploadedFile of this._customRootUploaders['Baseline'].uploadedFiles())
comparisonRootUploader.addUploadedFile(uploadedFile);this.enqueueToRender();}
render()
{super.render();const updateSelectedTestsLazily=this._renderTriggerableTestsLazily.evaluate();updateSelectedTestsLazily.evaluate(...this._selectedTests);const updateSelectedPlatformsLazily=this._renderTriggerablePlatformsLazily.evaluate(this._selectedTests,this._triggerablePlatforms);if(updateSelectedPlatformsLazily)
updateSelectedPlatformsLazily.evaluate(this._selectedPlatform);const[triggerable,error]=this._updateTriggerableLazily.evaluate(this._selectedTests,this._selectedPlatform);this._renderRepositoryPanesLazily.evaluate(triggerable,error,this._selectedPlatform,this._repositoryGroupByConfiguration,this._showComparison);this.renderReplace(this.content('baseline-testability'),this._buildTestabilityList(this._commitSetMap['Baseline'],'Baseline',this._invalidRevisionsByConfiguration['Baseline']));this.renderReplace(this.content('comparison-testability'),!this._showComparison?null:this._buildTestabilityList(this._commitSetMap['Comparison'],'Comparison',this._invalidRevisionsByConfiguration['Comparison']));}
_renderTriggerableTests()
{const enabledTriggerables=Triggerable.all().filter((triggerable)=>!triggerable.isDisabled());const acceptedTests=new Set;for(const triggerable of enabledTriggerables){for(const test of triggerable.acceptedTests())
acceptedTests.add(test);}
const tests=[...acceptedTests].sort((testA,testB)=>{if(testA.fullName()==testB.fullName())
return 0;return testA.fullName()<testB.fullName()?-1:1;});return this._renderRadioButtonList(this.content('test-list'),'test',tests,this.selectTests.bind(this),(test)=>test.fullName());}
_renderTriggerablePlatforms(selectedTests,triggerablePlatforms)
{if(!selectedTests.length){this.content('platform-pane').style.display='none';return null;}
this.content('platform-pane').style.display=null;return this._renderRadioButtonList(this.content('platform-list'),'platform',triggerablePlatforms,(selectedPlatforms)=>{this.selectPlatform(selectedPlatforms.length?selectedPlatforms[0]:null);});}
_renderRadioButtonList(listContainer,name,objects,callback,labelForObject=(object)=>object.label())
{const listItems=[];let selectedListItems=[];const checkSelectedRadioButtons=(newSelectedListItems)=>{selectedListItems.forEach((item)=>{item.label.classList.remove('selected');item.radioButton.checked=false;});selectedListItems=newSelectedListItems;selectedListItems.forEach((item)=>{item.label.classList.add('selected');item.radioButton.checked=true;});}
const element=ComponentBase.createElement;this.renderReplace(listContainer,objects.map((object)=>{const radioButton=element('input',{type:'radio',name:name,onchange:()=>{checkSelectedRadioButtons(listItems.filter((item)=>item.radioButton.checked));callback(selectedListItems.map((item)=>item.object));this.enqueueToRender();}});const label=element('label',[radioButton,labelForObject(object)]);listItems.push({radioButton,label,object});return element('li',label);}));return new LazilyEvaluatedFunction((...selectedObjects)=>{const objects=new Set(selectedObjects);checkSelectedRadioButtons(listItems.filter((item)=>objects.has(item.object)));});}
_updateTriggerable(tests,platform)
{let triggerable=null;let error=null;if(tests.length&&platform){triggerable=Triggerable.findByTestConfiguration(tests[0],platform);let matchingTests=new Set;let mismatchingTests=new Set;for(let test of tests){if(Triggerable.findByTestConfiguration(test,platform)==triggerable)
matchingTests.add(test);else
mismatchingTests.add(test);}
if(matchingTests.size<tests.length){const matchingTestNames=[...matchingTests].map((test)=>test.fullName()).sort().join('", "');const mismathingTestNames=[...mismatchingTests].map((test)=>test.fullName()).sort().join('", "');error=`Tests "${matchingTestNames}" and "${mismathingTestNames}" cannot be scheduled
                    simultenosuly on "${platform.label()}". Please select one of them at a time.`;}}
return[triggerable,error];}
_updateRepositoryGroups(triggerable)
{const repositoryGroups=triggerable?TriggerableRepositoryGroup.sortByNamePreferringSmallerRepositories(triggerable.repositoryGroups()):[];for(let name in this._repositoryGroupByConfiguration){const currentGroup=this._repositoryGroupByConfiguration[name];let matchingGroup=null;if(currentGroup){if(repositoryGroups.includes(currentGroup))
matchingGroup=currentGroup;else
matchingGroup=repositoryGroups.find((group)=>group.name()==currentGroup.name());}
if(!matchingGroup&&repositoryGroups.length)
matchingGroup=repositoryGroups[0];this._repositoryGroupByConfiguration[name]=matchingGroup;}}
_updateCommitSetMap()
{const newBaseline=this._computeCommitSet('Baseline');let newComparison=this._computeCommitSet('Comparison');if(newBaseline&&newComparison&&newBaseline.equals(newComparison))
newComparison=null;const currentBaseline=this._commitSetMap['Baseline'];const currentComparison=this._commitSetMap['Comparison'];const areCommitSetsEqual=(commitSetA,commitSetB)=>commitSetA==commitSetB||(commitSetA&&commitSetB&&commitSetA.equals(commitSetB));const sameBaselineConfig=areCommitSetsEqual(currentBaseline,newBaseline);const sameComparisionConfig=areCommitSetsEqual(currentComparison,newComparison);if(sameBaselineConfig&&sameComparisionConfig)
return;this._commitSetMap={'Baseline':newBaseline,'Comparison':newComparison};this.dispatchAction('testConfigChange');this.enqueueToRender();}
_computeCommitSet(configurationName)
{const repositoryGroup=this._repositoryGroupByConfiguration[configurationName];if(!repositoryGroup)
return null;const fileUploader=this._customRootUploaders[configurationName];if(!fileUploader||fileUploader.hasFileToUpload())
return null;const commitSet=new CustomCommitSet;for(let repository of repositoryGroup.repositories()){let revision=this._specifiedRevisions[configurationName].get(repository);const commit=this._fetchedCommits[configurationName].get(repository);if(commit){const commitLabel=commit.label();if(!revision||commit.revision().startsWith(revision)||commitLabel.startsWith(revision)||revision.startsWith(commitLabel))
revision=commit.revision();}
if(!revision)
return null;let patch=null;if(repositoryGroup.acceptsPatchForRepository(repository)){const uploaderMap=this._patchUploaders[configurationName];const uploader=uploaderMap.get(repository);if(uploader){const files=uploader.uploadedFiles();console.assert(files.length<=1);if(files.length)
patch=files[0];}}
commitSet.setRevisionForRepository(repository,revision,patch);}
for(let uploadedFile of fileUploader.uploadedFiles())
commitSet.addCustomRoot(uploadedFile);return commitSet;}
async _fetchCommitsForConfiguration(configurationName)
{const commitSet=this._commitSetMap[configurationName];if(!commitSet)
return;const specifiedRevisions=this._specifiedRevisions[configurationName];const fetchedCommits=this._fetchedCommits[configurationName];const invalidRevisionForRepository=this._invalidRevisionsByConfiguration[configurationName];await Promise.all(Array.from(commitSet.repositories()).map((repository)=>{const revision=commitSet.revisionForRepository(repository);return this._resolveRevision(repository,revision,specifiedRevisions,invalidRevisionForRepository,fetchedCommits);}));const latestCommitSet=this._commitSetMap[configurationName];if(commitSet!=latestCommitSet)
return;this.enqueueToRender();}
async _resolveRevision(repository,revision,specifiedRevisions,invalidRevisionForRepository,fetchedCommits)
{const fetchedCommit=fetchedCommits.get(repository);const specifiedRevision=specifiedRevisions.get(repository);if(fetchedCommit&&fetchedCommit.revision()==revision&&(!specifiedRevision||specifiedRevision==revision))
return;fetchedCommits.delete(repository);let commits=[];const revisionToFetch=specifiedRevision||revision;try{commits=await CommitLog.fetchForSingleRevision(repository,revisionToFetch,true);}catch(error){console.assert(error=='UnknownCommit'||error=='AmbiguousRevisionPrefix');if(revisionToFetch!=specifiedRevisions.get(repository))
return;invalidRevisionForRepository.set(repository,`"${revisionToFetch}": ${error == 'UnknownCommit' ? 'Invalid revision' : 'Ambiguous revision prefix'}`);return;}
console.assert(commits.length,1);if(revisionToFetch!=specifiedRevisions.get(repository))
return;invalidRevisionForRepository.delete(repository);fetchedCommits.set(repository,commits[0]);if(revisionToFetch!=commits[0].revision())
this._updateCommitSetMap();}
_renderRepositoryPanes(triggerable,error,platform,repositoryGroupByConfiguration,showComparison)
{this.content('repository-configuration-error-pane').style.display=error?null:'none';this.content('error').textContent=error;this.content('baseline-configuration-pane').style.display=triggerable?null:'none';this.content('specify-comparison-pane').style.display=triggerable&&!showComparison?null:'none';this.content('comparison-configuration-pane').style.display=triggerable&&showComparison?null:'none';if(!triggerable)
return;const visibleRepositoryGroups=triggerable.repositoryGroups().filter((group)=>!group.isHidden());const repositoryGroups=TriggerableRepositoryGroup.sortByNamePreferringSmallerRepositories(visibleRepositoryGroups);const repositorySet=new Set;for(let group of repositoryGroups){for(let repository of group.repositories())
repositorySet.add(repository);}
const repositories=Repository.sortByNamePreferringOnesWithURL([...repositorySet]);const requiredRepositories=repositories.filter((repository)=>{return repositoryGroups.every((group)=>group.repositories().includes(repository));});const alwaysAcceptsCustomRoots=repositoryGroups.every((group)=>group.acceptsCustomRoots());this._renderBaselineRevisionTable(platform,repositoryGroups,requiredRepositories,repositoryGroupByConfiguration,alwaysAcceptsCustomRoots);if(showComparison)
this._renderComparisonRevisionTable(platform,repositoryGroups,requiredRepositories,repositoryGroupByConfiguration,alwaysAcceptsCustomRoots);}
_renderBaselineRevisionTable(platform,repositoryGroups,requiredRepositories,repositoryGroupByConfiguration,alwaysAcceptsCustomRoots)
{let currentGroup=repositoryGroupByConfiguration['Baseline'];const optionalRepositoryList=this._optionalRepositoryList(currentGroup,requiredRepositories);this.renderReplace(this.content('baseline-revision-table'),this._buildRevisionTable('Baseline',repositoryGroups,currentGroup,platform,requiredRepositories,optionalRepositoryList,alwaysAcceptsCustomRoots));}
_renderComparisonRevisionTable(platform,repositoryGroups,requiredRepositories,repositoryGroupByConfiguration,alwaysAcceptsCustomRoots)
{let currentGroup=repositoryGroupByConfiguration['Comparison'];const optionalRepositoryList=this._optionalRepositoryList(currentGroup,requiredRepositories);this.renderReplace(this.content('comparison-revision-table'),this._buildRevisionTable('Comparison',repositoryGroups,currentGroup,platform,requiredRepositories,optionalRepositoryList,alwaysAcceptsCustomRoots));}
_optionalRepositoryList(currentGroup,requiredRepositories)
{if(!currentGroup)
return[];return Repository.sortByNamePreferringOnesWithURL(currentGroup.repositories().filter((repository)=>!requiredRepositories.includes(repository)));}
_buildRevisionTable(configurationName,repositoryGroups,currentGroup,platform,requiredRepositories,optionalRepositoryList,alwaysAcceptsCustomRoots)
{const element=ComponentBase.createElement;const customRootsTBody=element('tbody',[element('tr',[element('th','Roots'),element('td',this._customRootUploaders[configurationName]),]),]);return[element('tbody',requiredRepositories.map((repository)=>{return element('tr',[element('th',repository.name()),element('td',this._buildRevisionInput(configurationName,repository,platform))]);})),alwaysAcceptsCustomRoots?customRootsTBody:[],element('tbody',[element('tr',{'class':'group-row'},element('td',{colspan:2},this._buildRepositoryGroupList(repositoryGroups,currentGroup,configurationName))),]),!alwaysAcceptsCustomRoots&&currentGroup&&currentGroup.acceptsCustomRoots()?customRootsTBody:[],element('tbody',optionalRepositoryList.map((repository)=>{let uploader=currentGroup.acceptsPatchForRepository(repository)?this._ensurePatchUploader(configurationName,repository):null;return element('tr',[element('th',repository.name()),element('td',[this._buildRevisionInput(configurationName,repository,platform),uploader||[],])]);}))];}
_buildTestabilityList(commitSet,configurationName,invalidRevisionForRepository)
{const element=ComponentBase.createElement;const entries=[];if(!commitSet||!commitSet.repositories().length)
return[];for(const repository of commitSet.repositories()){const commit=this._fetchedCommits[configurationName].get(repository);if(commit&&commit.testability()&&!invalidRevisionForRepository.has(repository))
entries.push(element('li',`${commit.repository().name()} - "${commit.label()}": ${commit.testability()}`));if(invalidRevisionForRepository.has(repository))
entries.push(element('li',`${repository.name()} - ${invalidRevisionForRepository.get(repository)}`));}
return entries;}
_buildRepositoryGroupList(repositoryGroups,currentGroup,configurationName)
{const element=ComponentBase.createElement;return repositoryGroups.map((group)=>{const input=element('input',{type:'radio',name:'repositoryGroup-for-'+configurationName.toLowerCase(),checked:currentGroup==group,onchange:()=>this._selectRepositoryGroup(configurationName,group)});return[element('label',[input,group.description()])];});}
_selectRepositoryGroup(configurationName,group)
{const source=this._repositoryGroupByConfiguration;const clone={};for(let key in source)
clone[key]=source[key];clone[configurationName]=group;this._repositoryGroupByConfiguration=clone;this._updateCommitSetMap();this._fetchCommitsForConfiguration(configurationName);this.enqueueToRender();}
_buildRevisionInput(configurationName,repository,platform)
{const revision=this._specifiedRevisions[configurationName].get(repository)||'';const element=ComponentBase.createElement;let scheduledUpdate=null;const input=element('input',{value:revision,oninput:()=>{unmodifiedInput=null;const revisionToFetch=input.value;this._specifiedRevisions[configurationName].set(repository,revisionToFetch);this._updateCommitSetMap();if(scheduledUpdate)
clearTimeout(scheduledUpdate);scheduledUpdate=setTimeout(()=>{if(revisionToFetch==input.value)
this._fetchCommitsForConfiguration(configurationName);scheduledUpdate=null;},CustomAnalysisTaskConfigurator.commitFetchInterval);}});let unmodifiedInput=input;if(!revision){CommitLog.fetchLatestCommitForPlatform(repository,platform).then((commit)=>{if(commit&&unmodifiedInput){unmodifiedInput.value=commit.revision();this._fetchedCommits[configurationName].set(repository,commit);this._updateCommitSetMap();}});}
return input;}
static htmlTemplate()
{return` <section id="test-pane" class="pane"> <h2>1. Select a Test</h2> <ul id="test-list" class="config-list"></ul> </section> <section id="platform-pane" class="pane"> <h2>2. Select a Platform</h2> <ul id="platform-list" class="config-list"></ul> </section> <section id="repository-configuration-error-pane" class="pane"> <h2>Incompatible tests</h2> <p id="error"></p> </section> <section id="baseline-configuration-pane" class="pane"> <h2>3. Configure Baseline</h2> <table id="baseline-revision-table" class="revision-table"></table> <ul id="baseline-testability"></ul> </section> <section id="specify-comparison-pane" class="pane"> <button id="specify-comparison-button">Configure to Compare</button> </section> <section id="comparison-configuration-pane" class="pane"> <h2>4. Configure Comparison</h2> <table id="comparison-revision-table" class="revision-table"></table> <ul id="comparison-testability"></ul> </section>`;}
static cssTemplate()
{return` :host{display:flex!important;flex-direction:row!important}.pane{margin-right:1rem;padding:0}.pane h2{padding:0;margin:0;margin-bottom:0.5rem;font-size:1.2rem;font-weight:inherit;text-align:center;white-space:nowrap}.config-list{height:20rem;overflow:scroll;display:block;margin:0;padding:0;list-style:none;font-size:inherit;font-weight:inherit;border:none;border-top:solid 1px #ddd;border-bottom:solid 1px #ddd;white-space:nowrap}#platform-list:empty:before{content:"No matching platform";display:block;margin:1rem 0.5rem;text-align:center}.config-list label{display:block;padding:0.1rem 0.2rem}.config-list label:hover,.config-list a:hover{background:rgba(204,153,51,0.1)}.config-list label.selected,.config-list a.selected{background:#eee}.config-list a{display:block;padding:0.1rem 0.2rem;text-decoration:none;color:inherit}#repository-configuration-pane{position:relative}#repository-configuration-pane>button{margin-left:19.5rem}.revision-table{border:none;border-collapse:collapse;font-size:1rem}.revision-table thead{font-size:1.2rem}.revision-table tbody:empty{display:none}.revision-table tbody td,.revision-table tbody th{border-top:solid 1px #ddd;padding-top:0.5rem;padding-bottom:0.5rem}.revision-table td,.revision-table th{width:15rem;height:1.8rem;padding:0 0.2rem;border:none;font-weight:inherit}.revision-table thead th{text-align:center}.revision-table th close-button{vertical-align:bottom}.revision-table td:first-child,.revision-table th:first-child{width:6rem}.revision-table tr.group-row td{padding-left:5rem}label{white-space:nowrap;display:block}input:not([type=radio]){width:calc(100% - 0.6rem);padding:0.1rem 0.2rem;font-size:0.9rem;font-weight:inherit}#specify-comparison-pane button{margin-top:1.5rem;font-size:1.1rem;font-weight:inherit}#start-pane button{margin-top:1.5rem;font-size:1.2rem;font-weight:inherit}#baseline-testability li,#comparison-testability li{color:#c33;width:20rem}`;}}
CustomAnalysisTaskConfigurator.commitFetchInterval=100;ComponentBase.defineElement('custom-analysis-task-configurator',CustomAnalysisTaskConfigurator);class CustomConfigurationTestGroupForm extends TestGroupForm{constructor()
{super('custom-configuration-test-group-form');this._hasTask=false;this._updateTestGroupNameLazily=new LazilyEvaluatedFunction(this._updateTestGroupName.bind(this));}
setHasTask(hasTask)
{this._hasTask=hasTask;this.enqueueToRender();}
hasCommitSets()
{return!!this.part('configurator').commitSets();}
setConfigurations(test,platform,repetitionCount,commitSets)
{const configurator=this.part('configurator');configurator.selectTests([test]);configurator.selectPlatform(platform);if(commitSets.length==2)
configurator.setCommitSets(commitSets[0],commitSets[1]);this.setRepetitionCount(repetitionCount);this.enqueueToRender();}
startTesting()
{const taskName=this.content('task-name').value;const testGroupName=this.content('group-name').value;const configurator=this.part('configurator');const commitSets=configurator.commitSets();const platform=configurator.platform();const test=configurator.tests()[0];const repetitionType=this.part('repetition-type-selector').selectedRepetitionType;console.assert(!!this._hasTask===!taskName);if(!this._hasTask)
this.dispatchAction('startTesting',testGroupName,this._repetitionCount,repetitionType,commitSets,platform,test,this._notifyOnCompletion,taskName);else
this.dispatchAction('startTesting',testGroupName,this._repetitionCount,repetitionType,commitSets,platform,test,this._notifyOnCompletion);}
didConstructShadowTree()
{super.didConstructShadowTree();const configurator=this.part('configurator');configurator.listenToAction('testConfigChange',()=>{const tests=configurator.tests();const platform=configurator.platform();if(platform&&tests.length)
this.setTestAndPlatform(tests[0],platform);this.enqueueToRender();});this.content('task-name').oninput=()=>this.enqueueToRender();this.content('group-name').oninput=()=>this.enqueueToRender();}
render()
{super.render();const configurator=this.part('configurator');this._updateTestGroupNameLazily.evaluate(configurator.tests(),configurator.platform());const needsTaskName=!this._hasTask&&!this.content('task-name').value;this.content('iteration-start-pane').style.display=!!configurator.commitSets()?null:'none';this.content('task-name').style.display=this._hasTask?'none':null;this.content('start-button').disabled=needsTaskName||!this.content('group-name').value;}
_updateTestGroupName(tests,platform)
{if(!platform||!tests.length)
return;this.content('group-name').value=`${tests.map((test) => test.name()).join(', ')} on ${platform.label()}`;}
static cssTemplate()
{return super.cssTemplate()+` input{width:30%;min-width:10rem;font-size:1rem;font-weight:inherit}#form>*{margin-bottom:1rem}#start-button{display:block;margin-top:0.5rem;font-size:1.2rem;font-weight:inherit}`;}
static htmlTemplate()
{return`<form id="form"> <input id="task-name" type="text" placeholder="Name this task"> <custom-analysis-task-configurator id="configurator"></custom-analysis-task-configurator> <div id="iteration-start-pane"> <input id="group-name" type="text" placeholder="Test group name"> ${super.formContent()} <button id="start-button">Start</button> </div> </form>`;}}
ComponentBase.defineElement('custom-configuration-test-group-form',CustomConfigurationTestGroupForm);class InstantFileUploader extends ComponentBase{constructor()
{super('instant-file-uploader');this._fileInput=null;this._allowMultipleFiles=false;this._uploadedFiles=[];this._preuploadFiles=[];this._uploadProgress=new WeakMap;this._fileSizeFormatter=Metric.makeFormatter('B',3);this._renderUploadedFilesLazily=new LazilyEvaluatedFunction(this._renderUploadedFiles.bind(this));this._renderPreuploadFilesLazily=new LazilyEvaluatedFunction(this._renderPreuploadFiles.bind(this));}
clearUploads()
{this._uploadedFiles=[];this._preuploadFiles=[];this._uploadProgress=new WeakMap;}
hasFileToUpload(){return!!this._preuploadFiles.length;}
uploadedFiles(){return this._uploadedFiles;}
allowMultipleFiles()
{this._allowMultipleFiles=true;this.enqueueToRender();}
addUploadedFile(uploadedFile)
{console.assert(uploadedFile instanceof UploadedFile);if(this._uploadedFiles.includes(uploadedFile))
return;this._uploadedFiles.push(uploadedFile);this.enqueueToRender();}
didConstructShadowTree()
{const addButton=this.content('file-adder');addButton.onclick=()=>{inputElement.click();}
addButton.addEventListener('dragover',(event)=>{event.dataTransfer.dropEffect='copy';event.preventDefault();});addButton.addEventListener('drop',(event)=>{event.preventDefault();let files=event.dataTransfer.files;if(!files.length)
return;if(files.length>1&&!this._allowMultipleFiles)
files=[files[0]];this._uploadFiles(files);});const inputElement=document.createElement('input');inputElement.type='file';inputElement.onchange=()=>this._didFileInputChange(inputElement);this._fileInput=inputElement;}
render()
{this._renderUploadedFilesLazily.evaluate(...this._uploadedFiles);const uploadStatusElements=this._renderPreuploadFilesLazily.evaluate(...this._preuploadFiles);this._updateUploadStatus(uploadStatusElements);const fileCount=this._uploadedFiles.length+this._preuploadFiles.length;this.content('file-adder').style.display=this._allowMultipleFiles||!fileCount?null:'none';}
_renderUploadedFiles(...uploadedFiles)
{const element=ComponentBase.createElement;this.renderReplace(this.content('uploaded-files'),uploadedFiles.map((uploadedFile)=>{const authorInfo=uploadedFile.author()?' by '+uploadedFile.author():'';const createdAt=Metric.formatTime(uploadedFile.createdAt());const deleteButton=new CloseButton;deleteButton.listenToAction('activate',()=>this._removeUploadedFile(uploadedFile));return element('li',[deleteButton,element('code',{class:'filename'},uploadedFile.filename()),' ',element('small',{class:'filesize'},'('+this._fileSizeFormatter(uploadedFile.size())+')'),element('small',{class:'meta'},`Uploaded${authorInfo} on ${createdAt}`),]);}));}
_renderPreuploadFiles(...preuploadFiles)
{const element=ComponentBase.createElement;const uploadStatusElements=[];this.renderReplace(this.content('preupload-files'),preuploadFiles.map((file)=>{const progressBar=element('progress');const meta=element('small',{class:'meta'},progressBar);uploadStatusElements.push({file,meta,progressBar});return element('li',[element('code',file.name),' ',element('small',{class:'filesize'},'('+this._fileSizeFormatter(file.size)+')'),meta,]);}));return uploadStatusElements;}
_updateUploadStatus(uploadStatusElements)
{for(let entry of uploadStatusElements){const progress=this._uploadProgress.get(entry.file);const progressBar=entry.progressBar;if(!progress){progressBar.removeAttribute('max');progressBar.removeAttribute('value');return;}
if(progress.error){entry.meta.classList.add('hasError');entry.meta.textContent=this._formatUploadError(progress.error);}else{progressBar.max=progress.total;progressBar.value=progress.loaded;}}}
_formatUploadError(error)
{switch(error){case'NotSupported':return'Failed: File uploading is disabled';case'FileSizeLimitExceeded':return'Failed: The uploaded file was too big';case'FileSizeQuotaExceeded':return'Failed: Exceeded file upload quota';}
return'Failed to upload the file';}
_didFileInputChange(input)
{if(!input.files.length)
return;this._uploadFiles(input.files);input.value=null;this.enqueueToRender();}
_uploadFiles(files)
{const limit=UploadedFile.fileUploadSizeLimit;files=Array.from(files);for(let file of files){if(file.size>limit){alert(`The specified file "${file.name}" is too big (${this._fileSizeFormatter(file.size)}). It must be smaller than ${this._fileSizeFormatter(limit)}`);return;}}
const uploadProgress=this._uploadProgress;for(let file of files){UploadedFile.fetchUploadedFileWithIdenticalHash(file).then((uploadedFile)=>{if(uploadedFile){this._didUploadFile(file,uploadedFile);return;}
UploadedFile.uploadFile(file,(progress)=>{uploadProgress.set(file,progress);if(this._uploadProgress==uploadProgress)
this.enqueueToRender();}).then((uploadedFile)=>{if(this._uploadProgress==uploadProgress)
this._didUploadFile(file,uploadedFile);},(error)=>{uploadProgress.set(file,{error:error===0?'UnknownError':error});if(this._uploadedProgress==uploadProgress)
this.enqueueToRender();});});}
this._preuploadFiles=Array.from(files);}
_removeUploadedFile(uploadedFile)
{console.assert(uploadedFile instanceof UploadedFile);const index=this._uploadedFiles.indexOf(uploadedFile);if(index<0)
return;this._uploadedFiles.splice(index,1);this.dispatchAction('removedFile',uploadedFile);this.enqueueToRender();}
_didUploadFile(file,uploadedFile)
{console.assert(file instanceof File);const index=this._preuploadFiles.indexOf(file);if(index>=0)
this._preuploadFiles.splice(index,1);this._uploadedFiles.push(uploadedFile);this.dispatchAction('uploadedFile',uploadedFile);this.enqueueToRender();}
static htmlTemplate()
{return`<ul id="uploaded-files"></ul> <ul id="preupload-files"></ul> <button id="file-adder"><slot>Add a new file</slot></button>`;}
static cssTemplate()
{return` ul:empty{display:none}ul,li{margin:0;padding:0;list-style:none}li{position:relative;margin-bottom:0.25rem;padding-left:1.5rem;padding-bottom:0.25rem;border-bottom:solid 1px #eee}li:last-child{border-bottom:none}li>close-button{position:absolute;left:0;top:50%;margin-top:-0.5rem}li>progress{display:block}code{font-size:1.1rem;font-weight:inherit}small{font-size:0.8rem;font-weight:inherit;color:#666}small.meta{display:block}.hasError{color:#c60;font-weight:normal}`;}}
ComponentBase.defineElement('instant-file-uploader',InstantFileUploader);class FreshnessIndicator extends ComponentBase{constructor(lastDataPointDuration,testAgeTolerance,summary)
{super('freshness-indicator');this._lastDataPointDuration=lastDataPointDuration;this._testAgeTolerance=testAgeTolerance;this._highlighted=false;this._renderIndicatorLazily=new LazilyEvaluatedFunction(this._renderIndicator.bind(this));}
update(lastDataPointDuration,testAgeTolerance,highlighted)
{this._lastDataPointDuration=lastDataPointDuration;this._testAgeTolerance=testAgeTolerance;this._highlighted=highlighted;this.enqueueToRender();}
render()
{super.render();this._renderIndicatorLazily.evaluate(this._lastDataPointDuration,this._testAgeTolerance,this._url,this._highlighted);}
_renderIndicator(lastDataPointDuration,testAgeTolerance,url,highlighted)
{const element=ComponentBase.createElement;if(!lastDataPointDuration){this.renderReplace(this.content('container'),new SpinnerIcon);return;}
const hoursSinceLastDataPoint=this._lastDataPointDuration/3600/1000;const testAgeToleranceInHours=testAgeTolerance/3600/1000;const rating=1/(1+Math.exp(Math.log(1.2)*(hoursSinceLastDataPoint-testAgeToleranceInHours)));const hue=Math.round(120*rating);const brightness=Math.round(30+50*rating);const indicator=element('a',{id:'cell',class:`${highlighted ? 'highlight' : ''}`});indicator.style.backgroundColor=`hsl(${hue}, 100%, ${brightness}%)`;this.renderReplace(this.content('container'),indicator);}
static htmlTemplate()
{return`<div id='container'></div>`;}
static cssTemplate()
{return` div{margin:auto;height:1.8rem;width:1.8rem}a{display:block;height:1.6rem;width:1.6rem;border:0.1rem;border-color:white;border-style:solid;padding:0}a.highlight{height:1.4rem;width:1.4rem;border:0.2rem;border-style:solid;border-color:#0099ff}`;}}
ComponentBase.defineElement('freshness-indicator',FreshnessIndicator);class PlusButton extends ButtonBase{constructor()
{super('plus-button');}
static buttonContent()
{return`<g stroke="black" stroke-width="10" id="icon">
            <circle cx="50" cy="50" r="40" fill="transparent"/>
            <polygon points="50,25 50,75" />
            <polygon points="25,50 75,50" />
        </g>`;}}
ComponentBase.defineElement('plus-button',PlusButton);class MinusButton extends ButtonBase{constructor()
{super('minus-button');}
static buttonContent()
{return`<g stroke="black" stroke-width="10" id="icon">
            <circle cx="50" cy="50" r="40" fill="transparent"/>
            <polygon points="25,50 75,50" />
        </g>`;}}
ComponentBase.defineElement('minus-button',MinusButton);class ComboBox extends ComponentBase{constructor(candidates,maxCandidateListLength)
{super('combo-box');this._candidates=candidates;this._maxCandidateListLength=maxCandidateListLength||1000;this._candidateList=[];this._currentCandidateIndex=null;this._showCandidateList=false;this._renderCandidateListLazily=new LazilyEvaluatedFunction(this._renderCandidateList.bind(this));this._updateCandidateListLazily=new LazilyEvaluatedFunction(this._updateCandidateList.bind(this));}
didConstructShadowTree()
{super.didConstructShadowTree();const textField=this.content('text-field');textField.addEventListener('input',()=>{this._showCandidateList=true;this._currentCandidateIndex=null;this.enqueueToRender();});textField.addEventListener('change',()=>{this._autoCompleteIfOnlyOneMatchingItem();this._currentCandidateIndex=null;});textField.addEventListener('blur',()=>{this._showCandidateList=false;this._autoCompleteIfOnlyOneMatchingItem();this.enqueueToRender();});textField.addEventListener('focus',()=>{this._showCandidateList=true;this.enqueueToRender();});textField.addEventListener('keydown',(event)=>{if(event.key==='ArrowDown'||event.key==='ArrowUp')
this._moveCandidate(event.key==='ArrowDown');else if(event.key==='Tab'||event.key==='Enter'){let candidate=this._currentCandidateIndex===null?null:this._candidateList[this._currentCandidateIndex];if(!candidate&&this._candidateList.length===1)
candidate=this._candidateList[0];if(candidate)
this.dispatchAction('update',candidate);}});}
render()
{super.render();const candidateElementList=this._renderCandidateListLazily.evaluate(this._candidates,this.content('text-field').value);console.assert(this._currentCandidateIndex===null||(this._currentCandidateIndex>=0&&this._currentCandidateIndex<candidateElementList.length));const selectedCandidateElement=this._currentCandidateIndex===null?null:candidateElementList[this._currentCandidateIndex];this._updateCandidateListLazily.evaluate(selectedCandidateElement,this._showCandidateList);}
_autoCompleteIfOnlyOneMatchingItem()
{const textFieldValueInLowerCase=this.content('text-field').value.toLowerCase();if(!textFieldValueInLowerCase.length)
return;let matchingCandidateCount=0;let matchingCandidate=null;for(const candidate of this._candidates){if(candidate.toLowerCase().includes(textFieldValueInLowerCase)){matchingCandidateCount+=1;matchingCandidate=candidate;}
if(matchingCandidateCount>1)
break;}
if(matchingCandidateCount===1)
this.dispatchAction('update',matchingCandidate);}
_moveCandidate(forward)
{const candidateListLength=this._candidateList.length;if(!candidateListLength)
return;let newIndex=this._currentCandidateIndex;if(newIndex===null)
newIndex=forward?0:candidateListLength-1;else{newIndex+=forward?1:-1;if(newIndex>=this._candidateList.length)
newIndex=this._candidateList.length-1;if(newIndex<0)
newIndex=0;}
this._currentCandidateIndex=newIndex;this.enqueueToRender();}
_renderCandidateList(candidates,key)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;const candidatesStartingWithKey=[];const candidatesContainingKey=[];for(const candidate of candidates){const searchResult=candidate.toLowerCase().indexOf(key.toLowerCase());if(key&&searchResult<0)
continue;if(!searchResult)
candidatesStartingWithKey.push(candidate);else
candidatesContainingKey.push(candidate);}
this._candidateList=candidatesStartingWithKey.concat(candidatesContainingKey).slice(0,this._maxCandidateListLength);const candidateElementList=this._candidateList.map((candidate)=>{const item=link(candidate,()=>null);item.addEventListener('mousedown',()=>{this.dispatchAction('update',candidate);this.enqueueToRender();});return element('li',item);});this.renderReplace(this.content('candidate-list'),candidateElementList);return candidateElementList;}
_updateCandidateList(selectedCandidateElement,showCandidateList)
{const candidateList=this.content('candidate-list');candidateList.style.display=showCandidateList?null:'none';const previouslySelectedCandidateElement=candidateList.querySelector('.selected');if(previouslySelectedCandidateElement)
previouslySelectedCandidateElement.classList.remove('selected');if(selectedCandidateElement){selectedCandidateElement.className='selected';selectedCandidateElement.scrollIntoViewIfNeeded();}}
static htmlTemplate()
{return`<div id='combox-box'> <input id='text-field'></input> <ul id="candidate-list"></ul> </div>`;}
static cssTemplate()
{return` div{position:relative;height:1.4rem;left:0}ul:empty{display:none}ul{transition:background 250ms ease-in;margin:0;padding:0.1rem 0.3rem;list-style:none;background:rgba(255,255,255,0.95);border:solid 1px #ccc;top:1.5rem;border-radius:0.2rem;z-index:10;position:absolute;min-width:8.5rem;max-height:10rem;overflow:auto}li{text-align:left;margin:0;padding:0}li:hover,li.selected{background:rgba(204,153,51,0.1)}li a{text-decoration:none;font-size:0.8rem;color:inherit;display:block}`;}}
ComponentBase.defineElement('combo-box',ComboBox);class RepetitionTypeSelection extends ComponentBase{#triggerableConfiguration;#selectedRepetitionType;#renderRepetitionTypeListLazily;#disabled;constructor()
{super('repetition-type-selection');this.#selectedRepetitionType=null;this.#triggerableConfiguration=null;this.#disabled=false;this.#renderRepetitionTypeListLazily=new LazilyEvaluatedFunction(this._renderRepetitionTypeList.bind(this));}
didConstructShadowTree()
{const repetitionType=this.content('repetition-type');repetitionType.onchange=()=>this.#selectedRepetitionType=repetitionType.value;}
get selectedRepetitionType(){return this.#selectedRepetitionType;}
set selectedRepetitionType(repetitionType)
{console.assert(!this.#triggerableConfiguration||this.#triggerableConfiguration.isSupportedRepetitionType(repetitionType));this.#selectedRepetitionType=repetitionType;this.enqueueToRender();}
set disabled(value)
{console.assert(typeof value=='boolean');this.#disabled=value;this.enqueueToRender();}
setTestAndPlatform(test,platform)
{this.#triggerableConfiguration=TriggerableConfiguration.findByTestAndPlatform(test,platform);if(!this.#triggerableConfiguration)
this.#selectedRepetitionType=null;else if(!this.#triggerableConfiguration.isSupportedRepetitionType(this.#selectedRepetitionType))
this.#selectedRepetitionType=this.#triggerableConfiguration.supportedRepetitionTypes[0];this.enqueueToRender();}
labelForRepetitionType(repetitionType)
{return{'alternating':'alternating (ABAB)','sequential':'sequential (AABB)','paired-parallel':'parallel (paired A&B)',}[repetitionType];}
render()
{super.render();this.#renderRepetitionTypeListLazily.evaluate(this.#disabled,this.#selectedRepetitionType,...(this.#triggerableConfiguration?.supportedRepetitionTypes||[]));}
_renderRepetitionTypeList(disabled,selectedRepetitionType,...supportedRepetitionTypes)
{this.renderReplace(this.content('repetition-type'),supportedRepetitionTypes.map((repetitionType)=>this.createElement('option',{selected:repetitionType==selectedRepetitionType,value:repetitionType},this.labelForRepetitionType(repetitionType))));this.content('repetition-type').disabled=disabled;}
static htmlTemplate()
{return`<select id="repetition-type"></select>`;}}
ComponentBase.defineElement('repetition-type-selection',RepetitionTypeSelection);class Page extends ComponentBase{constructor(name)
{super('page-component');this._name=name;this._router=null;}
name(){return this._name;}
pageTitle(){return this.name();}
open(state)
{document.body.innerHTML='';document.body.appendChild(this.element());document.title=this.pageTitle();if(this._router)
this._router.pageDidOpen(this);this.updateFromSerializedState(state,true);this.enqueueToRender();}
enqueueToRender()
{if(this._router&&this._router.currentPage()!=this)
return;super.enqueueToRender();}
render()
{const title=this.pageTitle();if(document.title!=title)
document.title=title;super.render();}
setRouter(router){this._router=router;}
router(){return this._router;}
scheduleUrlStateUpdate(){this._router.scheduleUrlStateUpdate();}
routeName(){throw'NotImplemented';}
serializeState(){return{};}
updateFromSerializedState(state,isOpen){}}
class PageRouter{constructor()
{this._pages=[];this._defaultPage=null;this._currentPage=null;this._historyTimer=null;this._hash=null;window.onhashchange=this._hashDidChange.bind(this);}
addPage(page)
{this._pages.push(page);page.setRouter(this);}
setDefaultPage(defaultPage)
{this._defaultPage=defaultPage;}
currentPage(){return this._currentPage;}
route()
{let destinationPage=this._defaultPage;const parsed=this._deserializeFromHash(location.hash);if(parsed.route){let hashUrl=parsed.route;let bestMatchingRouteName=null;const queryIndex=hashUrl.indexOf('?');if(queryIndex>=0)
hashUrl=hashUrl.substring(0,queryIndex);for(const page of this._pages){const routeName=page.routeName();if(routeName==hashUrl){bestMatchingRouteName=routeName;destinationPage=page;break;}else if(hashUrl.startsWith(routeName)&&hashUrl.charAt(routeName.length)=='/'&&(!bestMatchingRouteName||bestMatchingRouteName.length<routeName.length)){bestMatchingRouteName=routeName;destinationPage=page;}}
if(bestMatchingRouteName)
parsed.state.remainingRoute=hashUrl.substring(bestMatchingRouteName.length+1);}
if(!destinationPage)
return false;if(this._currentPage!=destinationPage){this._currentPage=destinationPage;destinationPage.open(parsed.state);}else
destinationPage.updateFromSerializedState(parsed.state,false);destinationPage.enqueueToRender();return true;}
pageDidOpen(page)
{console.assert(page instanceof Page);const pageDidChange=this._currentPage!=page;this._currentPage=page;if(pageDidChange)
this.scheduleUrlStateUpdate();}
scheduleUrlStateUpdate()
{if(this._historyTimer)
return;this._historyTimer=setTimeout(this._updateURLState.bind(this),0);}
url(routeName,state)
{return this._serializeToHash(routeName,state);}
_updateURLState()
{this._historyTimer=null;console.assert(this._currentPage);const currentPage=this._currentPage;this._hash=this._serializeToHash(currentPage.routeName(),currentPage.serializeState());location.hash=this._hash;}
_hashDidChange()
{if(unescape(location.hash)==this._hash)
return;this.route();this._hash=null;}
_serializeToHash(route,state)
{const params=[];for(const key in state)
params.push(key+'='+this._serializeHashQueryValue(state[key]));const query=params.length?('?'+params.join('&')):'';return`#/${route}${query}`;}
_deserializeFromHash(hash)
{if(!hash||!hash.startsWith('#/'))
return{route:null,state:{}};hash=unescape(hash);const queryIndex=hash.indexOf('?');let route;const state={};if(queryIndex>=0){route=hash.substring(2,queryIndex);for(const part of hash.substring(queryIndex+1).split('&')){const keyValuePair=part.split('=');state[keyValuePair[0]]=this._deserializeHashQueryValue(keyValuePair[1]);}}else
route=hash.substring(2);return{route:route,state:state};}
_serializeHashQueryValue(value)
{if(value instanceof Array){const serializedItems=[];for(const item of value)
serializedItems.push(this._serializeHashQueryValue(item));return'('+serializedItems.join('-')+')';}
if(value instanceof Set)
return Array.from(value).sort().join('|');console.assert(value===null||value===undefined||typeof(value)==='number'||/[0-9]*/.test(value));return value===null||value===undefined?'null':value;}
_deserializeHashQueryValue(value)
{if(value.charAt(0)=='('){let nestingLevel=0;let end=0;let start=1;const result=[];for(const character of value){if(character=='(')
nestingLevel++;else if(character==')'){nestingLevel--;if(!nestingLevel)
break;}else if(nestingLevel==1&&character=='-'){result.push(this._deserializeHashQueryValue(value.substring(start,end)));start=end+1;}
end++;}
result.push(this._deserializeHashQueryValue(value.substring(start,end)));return result;}
if(value=='true')
return true;if(value=='false')
return true;if(value.match(/^[0-9\.]+$/))
return parseFloat(value);if(value.match(/^[A-Za-z][A-Za-z0-9|]*$/))
return new Set(value.toLowerCase().split('|'));return null;}
_countOccurrences(string,regex)
{const match=string.match(regex);return match?match.length:0;}}
class Heading extends ComponentBase{constructor()
{super('page-heading');this._title='';this._pageGroups=[];this._renderedCurrentPage=null;this._toolbar=null;this._toolbarChanged=false;this._router=null;}
title(){return this._title;}
setTitle(title){this._title=title;}
addPageGroup(group)
{for(var page of group)
page.setHeading(this);this._pageGroups.push(group);}
setToolbar(toolbar)
{console.assert(!toolbar||toolbar instanceof Toolbar);this._toolbar=toolbar;if(toolbar)
toolbar.setRouter(this._router);this._toolbarChanged=true;}
setRouter(router)
{this._router=router;if(this._toolbar)
this._toolbar.setRouter(router);}
render()
{console.assert(this._router);super.render();if(this._toolbarChanged){this.renderReplace(this.content().querySelector('.heading-toolbar'),this._toolbar?this._toolbar.element():null);this._toolbarChanged=false;}
if(this._toolbar)
this._toolbar.enqueueToRender();var currentPage=this._router.currentPage();if(this._renderedCurrentPage==currentPage)
return;this._renderedCurrentPage=currentPage;var title=this.content().querySelector('.heading-title a');title.textContent=this._title;var element=ComponentBase.createElement;var link=ComponentBase.createLink;var router=this._router;this.renderReplace(this.content().querySelector('.heading-navigation-list'),this._pageGroups.map(function(group){return element('ul',group.map(function(page){return element('li',{class:currentPage.belongsTo(page)?'selected':'',},link(page.name(),router.url(page.routeName(),page.serializeState())));}));}));}
static htmlTemplate()
{return` <nav class="heading-navigation" role="navigation"> <h1 class="heading-title"><a href="#"></a></h1> <div class="heading-navigation-list"></div> <div class="heading-toolbar"></div> </nav> `;}
static cssTemplate()
{return` .heading-navigation{position:relative;font-size:1rem;line-height:1rem}.heading-title{position:relative;z-index:2;margin:0;padding:1rem;border-bottom:solid 1px #ccc;background:#fff;color:#c93;font-size:1.5rem;font-weight:inherit}.heading-title a{text-decoration:none;color:inherit}.heading-navigation-list{display:block;white-space:nowrap;border-bottom:solid 1px #ccc;text-align:center;margin:0;margin-bottom:1rem;padding:0;padding-bottom:0.3rem}.heading-navigation-list ul{display:inline;margin:0;padding:0;margin-left:1rem;border-left:solid 1px #ccc;padding-left:1rem}.heading-navigation-list ul:first-child{border-left:none}.heading-navigation-list li{display:inline-block;position:relative;list-style:none;margin:0.3rem 0.5rem;padding:0}.heading-navigation-list a{text-decoration:none;color:inherit;color:#666}.heading-navigation-list a:hover{color:#369}.heading-navigation-list li.selected a{color:#000}.heading-navigation-list li.selected a:before{content:'';display:block;border:solid 5px #ccc;border-color:transparent transparent #ccc transparent;width:0px;height:0px;position:absolute;left:50%;margin-left:-5px;bottom:-0.55rem}.heading-toolbar{position:absolute;right:1rem;top:0.8rem;z-index:3}`;}}
class Toolbar extends ComponentBase{constructor(name)
{super(name);this._router=null;}
router(){return this._router;}
setRouter(router){this._router=router;}
static cssTemplate()
{return` .buttoned-toolbar{display:inline-block;margin:0 0;padding:0;font-size:0.9rem;border-radius:0.5rem}.buttoned-toolbar li>input{margin:0;border:solid 1px #ccc;border-radius:0.5rem;outline:none;font-size:inherit;padding:0.2rem 0.3rem;height:1rem}.buttoned-toolbar>input,.buttoned-toolbar>ul{margin-left:0.5rem}.buttoned-toolbar li{display:inline;margin:0;padding:0}.buttoned-toolbar li a{display:inline-block;border:solid 1px #ccc;font-weight:inherit;text-decoration:none;text-transform:capitalize;background:#fff;color:inherit;margin:0;margin-right:-1px;padding:0.2rem 0.3rem}.buttoned-toolbar input:focus,.buttoned-toolbar li.selected a{background:rgba(204,153,51,0.1)}.buttoned-toolbar li:not(.selected) a:hover{background:#eee}.buttoned-toolbar li:first-child a{border-top-left-radius:0.3rem;border-bottom-left-radius:0.3rem}.buttoned-toolbar li:last-child a{border-right-width:1px;border-top-right-radius:0.3rem;border-bottom-right-radius:0.3rem}`;}}
class PageWithHeading extends Page{constructor(name,toolbar)
{super(name);this._heading=null;this._parentPage=null;this._toolbar=toolbar;}
open(state)
{console.assert(this.heading());this.heading().setToolbar(this._toolbar);super.open(state);}
setParentPage(page){this._parentPage=page;}
belongsTo(page){return this==page||this._parentPage==page;}
setHeading(heading){this._heading=heading;}
heading(){return this._heading||this._parentPage.heading();}
toolbar(){return this._toolbar;}
title(){return this.name();}
pageTitle(){return this.title()+' - '+this.heading().title();}
render()
{console.assert(this.heading());if(document.body.firstChild!=this.heading().element())
document.body.insertBefore(this.heading().element(),document.body.firstChild);super.render();this.heading().enqueueToRender();}
static htmlTemplate()
{return`<section class="page-with-heading"></section>`;}}
class DomainControlToolbar extends Toolbar{constructor(name,numberOfDays)
{super(name);this._startTime=null;this._numberOfDays=numberOfDays;this._setByUser=false;this._callback=null;this._present=Date.now();this._millisecondsPerDay=24*3600*1000;}
startTime(){return this._startTime||(this._present-this._numberOfDays*this._millisecondsPerDay);}
endTime(){return this._present;}
setByUser(){return this._setByUser;}
setStartTime(startTime)
{this.setNumberOfDays(Math.max(1,Math.round((this._present-startTime)/this._millisecondsPerDay)));this._startTime=startTime;}
numberOfDays()
{return this._numberOfDays;}
setNumberOfDays(numberOfDays,setByUser)
{if(!numberOfDays)
return;this._startTime=null;this._numberOfDays=numberOfDays;this._setByUser=!!setByUser;}}
class DashboardToolbar extends DomainControlToolbar{constructor()
{var options=[{label:'1D',days:1},{label:'1W',days:7},{label:'2W',days:14},{label:'1M',days:30},{label:'3M',days:90},];super('dashboard-toolbar',options[1].days);this._options=options;this._currentOption=this._options[1];}
setNumberOfDays(days)
{if(!days)
days=7;this._currentOption=this._options[this._options.length-1];for(var option of this._options){if(days<=option.days){this._currentOption=option;break;}}
super.setNumberOfDays(this._currentOption.days);}
render()
{console.assert(this.router());var currentPage=this.router().currentPage();console.assert(currentPage);console.assert(currentPage instanceof DashboardPage);super.render();var element=ComponentBase.createElement;var link=ComponentBase.createLink;var self=this;var router=this.router();var currentOption=this._currentOption;this.renderReplace(this.content().querySelector('.dashboard-toolbar'),element('ul',{class:'buttoned-toolbar'},this._options.map(function(option){return element('li',{class:option==currentOption?'selected':'',},link(option.label,router.url(currentPage.routeName(),{numberOfDays:option.days})));})));}
static htmlTemplate()
{return`<div class="dashboard-toolbar"></div>`;}}
class DashboardPage extends PageWithHeading{constructor(name,table,toolbar)
{console.assert(toolbar instanceof DashboardToolbar);super(name,toolbar);this._table=table;this._charts=[];this._needsTableConstruction=true;this._needsStatusUpdate=true;this._statusViews=[];this._startTime=Date.now()-60*24*3600*1000;this._endTime=Date.now();this._tableGroups=null;}
routeName(){return`dashboard/${this._name}`;}
serializeState()
{return{numberOfDays:this.toolbar().numberOfDays()};}
updateFromSerializedState(state,isOpen)
{if(!isOpen||state.numberOfDays)
this.toolbar().setNumberOfDays(state.numberOfDays);var startTime=this.toolbar().startTime();var endTime=this.toolbar().endTime();for(var chart of this._charts)
chart.setDomain(startTime,endTime);this._needsTableConstruction=true;if(!isOpen)
this.enqueueToRender();}
open(state)
{if(!this._tableGroups){var columnCount=0;var tableGroups=[];for(var row of this._table){if(!row.some(function(cell){return cell instanceof Array;})){tableGroups.push([]);row=[''].concat(row);}
tableGroups[tableGroups.length-1].push(row);columnCount=Math.max(columnCount,row.length);}
for(var group of tableGroups){for(var row of group){for(var i=0;i<row.length;i++){if(row[i]instanceof Array)
row[i]=this._createChartForCell(row[i]);}
while(row.length<columnCount)
row.push([]);}}
this._tableGroups=tableGroups;}
super.open(state);}
render()
{super.render();console.assert(this._tableGroups);var element=ComponentBase.createElement;var link=ComponentBase.createLink;if(this._needsTableConstruction){var tree=[];var router=this.router();var startTime=this.toolbar().startTime();for(var group of this._tableGroups){tree.push(element('thead',element('tr',group[0].map(function(cell,cellIndex){if(!cellIndex)
return element('th',{class:'heading-column'});return element('td',cell.content||cell);}))));tree.push(element('tbody',group.slice(1).map(function(row){return element('tr',row.map(function(cell,cellIndex){if(!cellIndex)
return element('th',element('span',{class:'vertical-label'},cell));if(!cell.chart)
return element('td',cell);var url=router.url('charts',ChartsPage.createStateForDashboardItem(cell.platform.id(),cell.metric.id(),startTime));return element('td',[cell.statusView,link(cell.chart.element(),cell.label,url)]);}));})));}
this.renderReplace(this.content().querySelector('.dashboard-table'),tree);this._needsTableConstruction=false;}
for(var chart of this._charts)
chart.enqueueToRender();if(this._needsStatusUpdate){for(var statusView of this._statusViews)
statusView.enqueueToRender();this._needsStatusUpdate=false;}}
_createChartForCell(cell)
{console.assert(this.router());var platformId=cell[0];var metricId=cell[1];if(!platformId||!metricId)
return'';var result=ChartStyles.resolveConfiguration(platformId,metricId);if(result.error)
return result.error;var options=ChartStyles.dashboardOptions(result.metric.makeFormatter(3));let chart=new TimeSeriesChart(ChartStyles.createSourceList(result.platform,result.metric,false,false,true),options);chart.listenToAction('dataChange',()=>this._fetchedData())
this._charts.push(chart);var statusView=new DashboardChartStatusView(result.metric,chart);this._statusViews.push(statusView);return{chart:chart,statusView:statusView,platform:result.platform,metric:result.metric,label:result.metric.fullName()+' on '+result.platform.label()};}
_fetchedData()
{if(this._needsStatusUpdate)
return;this._needsStatusUpdate=true;setTimeout(()=>{this.enqueueToRender();},10);}
static htmlTemplate()
{return`<section class="page-with-heading"><table class="dashboard-table"></table></section>`;}
static cssTemplate()
{return` .dashboard-table{table-layout:fixed}.dashboard-table td,.dashboard-table th{border:none;text-align:center}.dashboard-table th,.dashboard-table thead td{color:#f96;font-weight:inherit;font-size:1.1rem;text-align:center;padding:0.2rem 0.4rem}.dashboard-table th{height:10rem;width:2rem;position:relative}.dashboard-table .heading-column{width:2rem;height:1rem}.dashboard-table th .vertical-label{position:absolute;left:0;right:0;display:block;-webkit-transform:rotate(-90deg) translate(-50%,0);-webkit-transform-origin:0 0;transform:rotate(-90deg) translate(-50%,0);transform-origin:0 0;width:10rem;height:2rem;line-height:1.8rem}table.dashboard-table{width:100%;height:100%;border:0}.dashboard-table td time-series-chart{height:10rem}.dashboard-table td>chart-status-view{display:block;width:100%}.dashboard-table td *:first-child{margin:0 0 0.2rem 0}`;}}
class ChartPaneStatusView extends ComponentBase{constructor(metric,chart)
{super('chart-pane-status-view');this._chart=chart;this._status=new ChartStatusEvaluator(chart,metric);this._revisionRange=new ChartRevisionRange(chart);this._currentRepository=null;this._renderStatusLazily=new LazilyEvaluatedFunction(this._renderStatus.bind(this));this._renderBuildRevisionTableLazily=new LazilyEvaluatedFunction(this._renderBuildRevisionTable.bind(this));}
render()
{super.render();this._renderStatusLazily.evaluate(this._status.status());const indicator=this._chart.currentIndicator();const build=indicator?indicator.point.build():null;this._renderBuildRevisionTableLazily.evaluate(build,this._currentRepository,this._revisionRange.revisionList());}
_renderStatus(status)
{status=status||{};let currentValue=status.currentValue||'';if(currentValue)
currentValue+=` (${status.valueDelta} / ${status.relativeDelta})`;this.content('current-value').textContent=currentValue;this.content('comparison').textContent=status.label||'';this.content('comparison').className=status.comparison||'';}
_renderBuildRevisionTable(build,currentRepository,revisionList)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;let tableContent=[];if(build){const url=build.url();const buildTag=build.buildTag();tableContent.push(element('tr',[element('td','Build'),element('td',{colspan:2},[url?link(buildTag,build.label(),url,true):buildTag,` (${Metric.formatTime(build.buildTime())})`]),]));}
if(revisionList){for(let info of revisionList){const selected=info.repository==this._currentRepository;const action=()=>{this.dispatchAction('openRepository',this._currentRepository==info.repository?null:info.repository);};tableContent.push(element('tr',{class:selected?'selected':''},[element('td',info.repository.name()),element('td',info.url?link(info.label,info.label,info.url,true):info.label),element('td',{class:'commit-viewer-opener'},link('\u00BB',action)),]));}}
this.renderReplace(this.content('build-revision'),tableContent);}
setCurrentRepository(repository)
{this._currentRepository=repository;this.enqueueToRender();}
static htmlTemplate()
{return` <div id="chart-pane-status"> <h3 id="current-value"></h3> <span id="comparison"></span> </div> <table id="build-revision"></table> `;}
static cssTemplate()
{return Toolbar.cssTemplate()+` #chart-pane-status{display:block;text-align:center}#current-value,#comparison{display:block;margin:0;padding:0;font-weight:normal;font-size:1rem}#comparison.worse{color:#c33}#comparison.better{color:#33c}#build-revision{line-height:1rem;font-size:0.9rem;font-weight:normal;padding:0;margin:0;margin-top:0.5rem;border-bottom:solid 1px #ccc;border-collapse:collapse;width:100%}#build-revision th,#build-revision td{font-weight:inherit;border-top:solid 1px #ccc;padding:0.2rem 0.2rem}#build-revision .selected>th,#build-revision .selected>td{background:rgba(204,153,51,0.1)}#build-revision .commit-viewer-opener{width:1rem}#build-revision .commit-viewer-opener a{text-decoration:none;color:inherit;font-weight:inherit}`;}}
function createTrendLineExecutableFromAveragingFunction(callback){return function(source,parameters){var timeSeries=source.measurementSet.fetchedTimeSeries(source.type,source.includeOutliers,source.extendToFuture);var values=timeSeries.values();if(!values.length)
return Promise.resolve(null);var averageValues=callback.call(null,values,...parameters);if(!averageValues)
return Promise.resolve(null);var interval=function(){return null;}
var result=new Array(averageValues.length);result.firstPoint=()=>result[0];result.nextPoint=(point)=>result[point.seriesIndex+1];for(var i=0;i<averageValues.length;i++)
result[i]={seriesIndex:i,time:timeSeries.findPointByIndex(i).time,value:averageValues[i],interval:interval};return Promise.resolve(result);}}
const ChartTrendLineTypes=[{id:0,label:'None',},{id:5,label:'Segmentation',execute:function(source,parameters){return source.measurementSet.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion',parameters,source.type,source.includeOutliers,source.extendToFuture).then(function(segmentation){if(!segmentation)
return segmentation;segmentation.forEach((point,index)=>point.seriesIndex=index);segmentation.firstPoint=()=>segmentation[0];segmentation.nextPoint=(point)=>segmentation[point.seriesIndex+1];return segmentation;});},parameterList:[{label:"Segment count weight",value:2.5,min:0.01,max:10,step:0.01},{label:"Grid size",value:500,min:100,max:10000,step:10}]},{id:6,label:'Segmentation with Welch\'s t-test change detection',execute:async function(source,parameters){const segmentation=await source.measurementSet.fetchSegmentation('segmentTimeSeriesByMaximizingSchwarzCriterion',parameters,source.type,source.includeOutliers,source.extendToFuture);if(!segmentation)
return segmentation;const metric=Metric.findById(source.measurementSet.metricId());const timeSeries=source.measurementSet.fetchedTimeSeries(source.type,source.includeOutliers,source.extendToFuture);segmentation.analysisAnnotations=Statistics.findRangesForChangeDetectionsWithWelchsTTest(timeSeries.values(),segmentation,parameters[parameters.length-1]).map((range)=>{const startPoint=timeSeries.findPointByIndex(range.startIndex);const endPoint=timeSeries.findPointByIndex(range.endIndex);const summary=metric.labelForDifference(range.segmentationStartValue,range.segmentationEndValue,'progression','regression');return{task:null,fillStyle:ChartStyles.annotationFillStyleForTask(null),startTime:startPoint.time,endTime:endPoint.time,label:`Potential ${summary.changeLabel}`,};});segmentation.firstPoint=()=>segmentation[0];segmentation.nextPoint=(point)=>segmentation[point.seriesIndex+1];return segmentation;},parameterList:[{label:"Segment count weight",value:2.5,min:0.01,max:10,step:0.01},{label:"Grid size",value:500,min:100,max:10000,step:10},{label:"t-test significance",value:0.99,options:Statistics.supportedOneSideTTestProbabilities()},]},{id:1,label:'Simple Moving Average',parameterList:[{label:"Backward window size",value:8,min:2,step:1},{label:"Forward window size",value:4,min:0,step:1}],execute:createTrendLineExecutableFromAveragingFunction(Statistics.movingAverage.bind(Statistics))},{id:2,label:'Cumulative Moving Average',execute:createTrendLineExecutableFromAveragingFunction(Statistics.cumulativeMovingAverage.bind(Statistics))},{id:3,label:'Exponential Moving Average',parameterList:[{label:"Smoothing factor",value:0.01,min:0.001,max:0.9,step:0.001},],execute:createTrendLineExecutableFromAveragingFunction(Statistics.exponentialMovingAverage.bind(Statistics))},];ChartTrendLineTypes.DefaultType=ChartTrendLineTypes[1];class ChartPane extends ChartPaneBase{constructor(chartsPage,platformId,metricId)
{super('chart-pane');this._mainChartIndicatorWasLocked=false;this._chartsPage=chartsPage;this._lockedPopover=null;this._trendLineType=null;this._trendLineParameters=[];this._trendLineVersion=0;this._renderedTrendLineOptions=false;this.configure(platformId,metricId);}
didConstructShadowTree()
{this.part('close').listenToAction('activate',()=>{this._chartsPage.closePane(this);});const createWithTestGroupCheckbox=this.content('create-with-test-group');const repetitionCount=this.content('confirm-repetition');const notifyOnCompletion=this.content('notify-on-completion');const repetitionTypeSelection=this.part('repetition-type-selector');createWithTestGroupCheckbox.onchange=()=>{const shouldDisable=!createWithTestGroupCheckbox.checked;repetitionCount.disabled=shouldDisable;notifyOnCompletion.disabled=shouldDisable;repetitionTypeSelection.disabled=shouldDisable;};}
serializeState()
{var state=[this._platformId,this._metricId];if(this._mainChart){var selection=this._mainChart.currentSelection();const indicator=this._mainChart.currentIndicator();if(selection)
state[2]=selection;else if(indicator&&indicator.isLocked)
state[2]=indicator.point.id;}
var graphOptions=new Set;if(!this.isSamplingEnabled())
graphOptions.add('noSampling');if(this.isShowingOutliers())
graphOptions.add('showOutliers');if(graphOptions.size)
state[3]=graphOptions;if(this._trendLineType)
state[4]=[this._trendLineType.id].concat(this._trendLineParameters);return state;}
updateFromSerializedState(state,isOpen)
{if(!this._mainChart)
return;var selectionOrIndicatedPoint=state[2];if(selectionOrIndicatedPoint instanceof Array)
this._mainChart.setSelection([parseFloat(selectionOrIndicatedPoint[0]),parseFloat(selectionOrIndicatedPoint[1])]);else if(typeof(selectionOrIndicatedPoint)=='number'){this._mainChart.setIndicator(selectionOrIndicatedPoint,true);this._mainChartIndicatorWasLocked=true;}else
this._mainChart.setIndicator(null,false);var graphOptions=state[3];if(graphOptions instanceof Set){this.setSamplingEnabled(!graphOptions.has('nosampling'));this.setShowOutliers(graphOptions.has('showoutliers'));}
var trendLineOptions=state[4];if(!(trendLineOptions instanceof Array))
trendLineOptions=[];var trendLineId=trendLineOptions[0];var trendLineType=ChartTrendLineTypes.find(function(type){return type.id==trendLineId;})||ChartTrendLineTypes.DefaultType;this._trendLineType=trendLineType;this._trendLineParameters=(trendLineType.parameterList||[]).map(function(parameter,index){var specifiedValue=parseFloat(trendLineOptions[index+1]);return!isNaN(specifiedValue)?specifiedValue:parameter.value;});this._updateTrendLine();this._renderedTrendLineOptions=false;
}
setOverviewSelection(selection)
{if(this._overviewChart)
this._overviewChart.setSelection(selection);}
_overviewSelectionDidChange(domain,didEndDrag)
{super._overviewSelectionDidChange(domain,didEndDrag);this._chartsPage.setMainDomainFromOverviewSelection(domain,this,didEndDrag);}
_mainSelectionDidChange(selection,didEndDrag)
{super._mainSelectionDidChange(selection,didEndDrag);this._chartsPage.mainChartSelectionDidChange(this,didEndDrag);}
_mainSelectionDidZoom(selection)
{super._mainSelectionDidZoom(selection);this._chartsPage.setMainDomainFromZoom(selection,this);}
router(){return this._chartsPage.router();}
openNewRepository(repository)
{this.content().querySelector('.chart-pane').focus();this._chartsPage.setOpenRepository(repository);}
_indicatorDidChange(indicatorID,isLocked)
{this._chartsPage.mainChartIndicatorDidChange(this,isLocked!=this._mainChartIndicatorWasLocked);this._mainChartIndicatorWasLocked=isLocked;super._indicatorDidChange(indicatorID,isLocked);}
async _analyzeRange(startPoint,endPoint)
{const router=this._chartsPage.router();const newWindow=window.open(router.url('analysis/task/create',{inProgress:true}),'_blank');const name=this.content('task-name').value;const createWithTestGroup=this.content('create-with-test-group').checked;const repetitionCount=this.content('confirm-repetition').value;const notifyOnCompletion=this.content('notify-on-completion').checked;const repetitionType=this.part('repetition-type-selector').selectedRepetitionType;try{const analysisTask=await(createWithTestGroup?AnalysisTask.create(name,startPoint,endPoint,'Confirm',repetitionCount,repetitionType,notifyOnCompletion):AnalysisTask.create(name,startPoint,endPoint));newWindow.location.href=router.url('analysis/task/'+analysisTask.id());this.fetchAnalysisTasks(true);}catch(error){newWindow.location.href=router.url('analysis/task/create',{error:error});}}
_markAsOutlier(markAsOutlier,points)
{var self=this;return Promise.all(points.map(function(point){return PrivilegedAPI.sendRequest('update-run-status',{'run':point.id,'markedOutlier':markAsOutlier});})).then(function(){self._mainChart.fetchMeasurementSets(true);},function(error){alert('Failed to update the outlier status: '+error);}).catch();}
render()
{if(this._platform&&this._metric){var metric=this._metric;var platform=this._platform;this.renderReplace(this.content().querySelector('.chart-pane-title'),metric.fullName()+' on '+platform.label());}
if(this._mainChartStatus)
this._renderActionToolbar();super.render();}
_renderActionToolbar()
{var actions=[];var platform=this._platform;var metric=this._metric;var element=ComponentBase.createElement;var link=ComponentBase.createLink;var self=this;if(this._chartsPage.canBreakdown(platform,metric)){actions.push(element('li',link('Breakdown',function(){self._chartsPage.insertBreakdownPanesAfter(platform,metric,self);})));}
var platformPopover=this.content().querySelector('.chart-pane-alternative-platforms');var alternativePlatforms=this._chartsPage.alternatePlatforms(platform,metric);if(alternativePlatforms.length){this.renderReplace(platformPopover,Platform.sortByName(alternativePlatforms).map(function(platform){return element('li',link(platform.label(),function(){self._chartsPage.insertPaneAfter(platform,metric,self);}));}));actions.push(this._makePopoverActionItem(platformPopover,'Other Platforms',true));}else
platformPopover.style.display='none';var analyzePopover=this.content().querySelector('.chart-pane-analyze-popover');const selectedPoints=this._mainChart.selectedPoints('current');const hasSelectedPoints=selectedPoints&&selectedPoints.length();if(hasSelectedPoints){this.part('repetition-type-selector').setTestAndPlatform(metric.test(),platform);actions.push(this._makePopoverActionItem(analyzePopover,'Analyze',false));analyzePopover.onsubmit=this.createEventHandler(()=>{this._analyzeRange(selectedPoints.firstPoint(),selectedPoints.lastPoint());});}else{analyzePopover.style.display='none';analyzePopover.onsubmit=this.createEventHandler(()=>{});}
var filteringOptions=this.content().querySelector('.chart-pane-filtering-options');actions.push(this._makePopoverActionItem(filteringOptions,'Filtering',true));var trendLineOptions=this.content().querySelector('.chart-pane-trend-line-options');actions.push(this._makePopoverActionItem(trendLineOptions,'Trend lines',true));this._renderFilteringPopover();this._renderTrendLinePopover();this._lockedPopover=null;this.renderReplace(this.content().querySelector('.chart-pane-action-buttons'),actions);}
_makePopoverActionItem(popover,label,shouldRespondToHover)
{var self=this;popover.anchor=ComponentBase.createLink(label,function(){var makeVisible=self._lockedPopover!=popover;self._setPopoverVisibility(popover,makeVisible);if(makeVisible)
self._lockedPopover=popover;});if(shouldRespondToHover)
this._makePopoverOpenOnHover(popover);return ComponentBase.createElement('li',{class:this._lockedPopover==popover?'selected':''},popover.anchor);}
_makePopoverOpenOnHover(popover)
{var mouseIsInAnchor=false;var mouseIsInPopover=false;var self=this;var closeIfNeeded=function(){setTimeout(function(){if(self._lockedPopover!=popover&&!mouseIsInAnchor&&!mouseIsInPopover)
self._setPopoverVisibility(popover,false);},0);}
popover.anchor.onmouseenter=function(){if(self._lockedPopover)
return;mouseIsInAnchor=true;self._setPopoverVisibility(popover,true);}
popover.anchor.onmouseleave=function(){mouseIsInAnchor=false;closeIfNeeded();}
popover.onmouseenter=function(){mouseIsInPopover=true;}
popover.onmouseleave=function(){mouseIsInPopover=false;closeIfNeeded();}}
_setPopoverVisibility(popover,visible)
{var anchor=popover.anchor;if(visible){var width=anchor.offsetParent.offsetWidth;popover.style.top=anchor.offsetTop+anchor.offsetHeight+'px';popover.style.right=(width-anchor.offsetLeft-anchor.offsetWidth)+'px';}
popover.style.display=visible?null:'none';anchor.parentNode.className=visible?'selected':'';if(this._lockedPopover&&this._lockedPopover!=popover&&visible)
this._setPopoverVisibility(this._lockedPopover,false);if(this._lockedPopover==popover&&!visible)
this._lockedPopover=null;}
_renderFilteringPopover()
{var enableSampling=this.content().querySelector('.enable-sampling');enableSampling.checked=this.isSamplingEnabled();enableSampling.onchange=function(){self.setSamplingEnabled(enableSampling.checked);self._chartsPage.graphOptionsDidChange();}
var showOutliers=this.content().querySelector('.show-outliers');showOutliers.checked=this.isShowingOutliers();showOutliers.onchange=function(){self.setShowOutliers(showOutliers.checked);self._chartsPage.graphOptionsDidChange();}
var markAsOutlierButton=this.content().querySelector('.mark-as-outlier');const indicator=this._mainChart.currentIndicator();let firstSelectedPoint=indicator&&indicator.isLocked?indicator.point:null;if(!firstSelectedPoint)
firstSelectedPoint=this._mainChart.firstSelectedPoint('current');var alreayMarkedAsOutlier=firstSelectedPoint&&firstSelectedPoint.markedOutlier;var self=this;markAsOutlierButton.textContent=(alreayMarkedAsOutlier?'Unmark':'Mark')+' selected points as outlier';markAsOutlierButton.onclick=function(){var selectedPoints=[firstSelectedPoint];if(self._mainChart.currentSelection('current'))
selectedPoints=self._mainChart.selectedPoints('current');self._markAsOutlier(!alreayMarkedAsOutlier,selectedPoints);}
markAsOutlierButton.disabled=!firstSelectedPoint;}
_renderTrendLinePopover()
{var element=ComponentBase.createElement;var link=ComponentBase.createLink;var self=this;const trendLineTypesContainer=this.content().querySelector('.trend-line-types');if(!trendLineTypesContainer.querySelector('select')){this.renderReplace(trendLineTypesContainer,[element('select',{onchange:this._trendLineTypeDidChange.bind(this)},ChartTrendLineTypes.map((type)=>{return element('option',{value:type.id},type.label);}))]);}
if(this._trendLineType)
trendLineTypesContainer.querySelector('select').value=this._trendLineType.id;if(this._renderedTrendLineOptions)
return;this._renderedTrendLineOptions=true;if(this._trendLineParameters.length){var configuredParameters=this._trendLineParameters;this.renderReplace(this.content().querySelector('.trend-line-parameter-list'),[element('h3','Parameters'),element('ul',this._trendLineType.parameterList.map(function(parameter,index){if(parameter.options){const select=element('select',parameter.options.map((option)=>element('option',{value:option,selected:option==parameter.value},option)));select.onchange=self._trendLineParameterDidChange.bind(self);select.parameterIndex=index;return element('li',element('label',[parameter.label+': ',select]));}
var attributes={type:'number'};for(var name in parameter)
attributes[name]=parameter[name];attributes.value=configuredParameters[index];const input=element('input',attributes);input.parameterIndex=index;input.oninput=self._trendLineParameterDidChange.bind(self);input.onchange=self._trendLineParameterDidChange.bind(self);return element('li',element('label',[parameter.label+': ',input]));}))]);}else
this.renderReplace(this.content().querySelector('.trend-line-parameter-list'),[]);}
_trendLineTypeDidChange(event)
{var newType=ChartTrendLineTypes.find(function(type){return type.id==event.target.value});if(newType==this._trendLineType)
return;this._trendLineType=newType;this._trendLineParameters=this._defaultParametersForTrendLine(newType);this._renderedTrendLineOptions=false;this._updateTrendLine();this._chartsPage.graphOptionsDidChange();this.enqueueToRender();}
_defaultParametersForTrendLine(type)
{return type&&type.parameterList?type.parameterList.map(function(parameter){return parameter.value;}):[];}
_trendLineParameterDidChange(event)
{var input=event.target;var index=input.parameterIndex;var newValue=parseFloat(input.value);if(this._trendLineParameters[index]==newValue)
return;this._trendLineParameters[index]=newValue;var self=this;setTimeout(function(){if(self._trendLineParameters[index]!=newValue)
return;self._updateTrendLine();self._chartsPage.graphOptionsDidChange();},500);}
_didFetchData()
{super._didFetchData();this._updateTrendLine();}
async _updateTrendLine()
{if(!this._mainChart.sourceList())
return;this._trendLineVersion++;var currentTrendLineType=this._trendLineType||ChartTrendLineTypes.DefaultType;var currentTrendLineParameters=this._trendLineParameters||this._defaultParametersForTrendLine(currentTrendLineType);var currentTrendLineVersion=this._trendLineVersion;var sourceList=this._mainChart.sourceList();if(!currentTrendLineType.execute){this._mainChart.clearTrendLines();this.enqueueToRender();}else{await Promise.all(sourceList.map(async(source,sourceIndex)=>{const trendlineSeries=await currentTrendLineType.execute.call(null,source,currentTrendLineParameters);if(this._trendLineVersion==currentTrendLineVersion)
this._mainChart.setTrendLine(sourceIndex,trendlineSeries);if(trendlineSeries&&trendlineSeries.analysisAnnotations)
this._detectedAnnotations=trendlineSeries.analysisAnnotations;else
this._detectedAnnotations=null;}));this.enqueueToRender();}}
static paneHeaderTemplate()
{return` <header class="chart-pane-header"> <h2 class="chart-pane-title">-</h2> <nav class="chart-pane-actions"> <ul> <li><close-button id="close"></close-button></li> </ul> <ul class="chart-pane-action-buttons buttoned-toolbar"></ul> <ul class="chart-pane-alternative-platforms popover" style="display:none"></ul> <form class="chart-pane-analyze-popover popover" style="display:none"> <input type="text" id="task-name" required> <button>Create</button> <li> <label><input type="checkbox" id="create-with-test-group" checked></label> <label>Confirm with</label> <select id="confirm-repetition"> <option>1</option> <option>2</option> <option>3</option> <option selected>4</option> <option>5</option> <option>6</option> <option>7</option> <option>8</option> <option>9</option> <option>10</option> </select> <label>iterations</label> </li> <li> <label>In</label> <repetition-type-selection id="repetition-type-selector"></repetition-type-selection> </li> <li> <label><input type="checkbox" id="notify-on-completion" checked>Notify on completion</label> </li> </form> <ul class="chart-pane-filtering-options popover" style="display:none"> <li><label><input type="checkbox" class="enable-sampling">Sampling</label></li> <li><label><input type="checkbox" class="show-outliers">Show outliers</label></li> <li><button class="mark-as-outlier">Mark selected points as outlier</button></li> </ul> <ul class="chart-pane-trend-line-options popover" style="display:none"> <div class="trend-line-types"></div> <div class="trend-line-parameter-list"></div> </ul> </nav> </header> `;}
static cssTemplate()
{return ChartPaneBase.cssTemplate()+` .chart-pane{border:solid 1px #ccc;border-radius:0.5rem;margin:1rem;margin-bottom:2rem}.chart-pane-body{height:calc(100% - 2rem)}.chart-pane-header{position:relative;left:0;top:0;width:100%;height:2rem;line-height:2rem;border-bottom:solid 1px #ccc}.chart-pane-title{margin:0 0.5rem;padding:0;padding-left:1.5rem;font-size:1rem;font-weight:inherit}.chart-pane-actions{position:absolute;display:flex;flex-direction:row;justify-content:space-between;align-items:center;width:100%;height:2rem;top:0;padding:0 0}.chart-pane-actions ul,form{display:block;padding:0;margin:0 0.5rem;font-size:1rem;line-height:1rem;list-style:none}.chart-pane-actions .chart-pane-action-buttons{font-size:0.9rem;line-height:0.9rem}.chart-pane-actions .popover{position:absolute;top:0;right:0;border:solid 1px #ccc;border-radius:0.2rem;z-index:10;padding:0.2rem 0;margin:0;margin-top:-0.2rem;margin-right:-0.2rem;background:rgba(255,255,255,0.95)}@supports ( -webkit-backdrop-filter:blur(0.5rem) ){.chart-pane-actions .popover{background:rgba(255,255,255,0.6);-webkit-backdrop-filter:blur(0.5rem)}}.chart-pane-actions .popover li{}.chart-pane-actions .popover li a{display:block;text-decoration:none;color:inherit;font-size:0.9rem;padding:0.2rem 0.5rem}.chart-pane-actions .popover a:hover,.chart-pane-actions .popover input:focus{background:rgba(204,153,51,0.1)}.chart-pane-actions .chart-pane-analyze-popover{padding:0.5rem}.chart-pane-actions .popover label{font-size:0.9rem}.chart-pane-actions .popover.chart-pane-filtering-options{padding:0.2rem}.chart-pane-actions .popover.chart-pane-trend-line-options h3{font-size:0.9rem;line-height:0.9rem;font-weight:inherit;margin:0;padding:0.2rem;border-bottom:solid 1px #ccc}.chart-pane-actions .popover.chart-pane-trend-line-options select,.chart-pane-actions .popover.chart-pane-trend-line-options label{margin:0.2rem}.chart-pane-actions .popover.chart-pane-trend-line-options label{font-size:0.8rem}.chart-pane-actions .popover.chart-pane-trend-line-options input{width:2.5rem}.chart-pane-actions .popover input[type=text]{font-size:1rem;width:15rem;outline:none;border:solid 1px #ccc}`;}}
class ChartsToolbar extends DomainControlToolbar{constructor()
{super('chars-toolbar',7);this._minDayCount=1;this._maxDayCount=366;this._numberOfDaysCallback=null;this._slider=this.content().querySelector('.slider');this._slider.addEventListener('change',this._sliderValueMayHaveChanged.bind(this));this._slider.addEventListener('mousemove',this._sliderValueMayHaveChanged.bind(this));this._editor=this.content().querySelector('.editor');this._editor.addEventListener('focus',this._enterTextMode.bind(this));this._editor.addEventListener('blur',this._exitTextMode.bind(this));this._editor.addEventListener('input',this._editorValueMayHaveChanged.bind(this));this._editor.addEventListener('change',this._editorValueMayHaveChanged.bind(this));this._labelSpan=this.content().querySelector('.day-count');this._addPaneCallback=null;this._paneSelector=this.content().querySelector('pane-selector').component();this._paneSelectorOpener=this.content().querySelector('.pane-selector-opener');this._paneSelectorContainer=this.content().querySelector('.pane-selector-container');this._paneSelector.setCallback(this._addPane.bind(this));this._paneSelectorOpener.addEventListener('click',this._togglePaneSelector.bind(this));this._paneSelectorContainer.style.display='none';}
render()
{super.render();this._paneSelector.enqueueToRender();this._labelSpan.textContent=this._numberOfDays;this._setInputElementValue(this._numberOfDays);}
setNumberOfDaysCallback(callback)
{console.assert(!callback||callback instanceof Function);this._numberOfDaysCallback=callback;}
setAddPaneCallback(callback)
{console.assert(!callback||callback instanceof Function);this._addPaneCallback=callback;}
setStartTime(startTime)
{this._exitTextMode();if(startTime)
super.setStartTime(startTime);else
super.setNumberOfDays(7);}
_setInputElementValue(value)
{this._slider.value=Math.pow(value,1/3);this._slider.min=Math.pow(this._minDayCount,1/3);this._slider.max=Math.pow(this._maxDayCount,1/3);this._slider.step='any';this._editor.value=value;}
_enterTextMode(event)
{event.preventDefault();this._editor.style.opacity=1;this._editor.style.marginLeft='-2.5rem';this._labelSpan.style.opacity=0;this._slider.style.opacity=0;}
_exitTextMode(event)
{if(event)
event.preventDefault();this._editor.style.opacity=0;this._editor.style.marginLeft=null;this._labelSpan.style.opacity=1;this._slider.style.opacity=1;}
_sliderValueMayHaveChanged(event)
{var numberOfDays=Math.round(Math.pow(parseFloat(this._slider.value),3));this._callNumberOfDaysCallback(event,numberOfDays);}
_editorValueMayHaveChanged(event)
{var rawNumber=Math.round(parseFloat(this._editor.value));var numberOfDays=Math.max(this._minDayCount,Math.min(this._maxDayCount,rawNumber));if(this._editor.value!=numberOfDays)
this._editor.value=numberOfDays;this._callNumberOfDaysCallback(event,numberOfDays);}
_callNumberOfDaysCallback(event,numberOfDays)
{var shouldUpdateState=event.type=='change';if((this.numberOfDays()!=numberOfDays||shouldUpdateState)&&this._numberOfDaysCallback)
this._numberOfDaysCallback(numberOfDays,shouldUpdateState);}
_togglePaneSelector(event)
{event.preventDefault();if(this._paneSelectorContainer.style.display=='none')
this._openPaneSelector(true);else
this._closePaneSelector();}
_openPaneSelector(shouldFocus)
{var opener=this._paneSelectorOpener;var container=this._paneSelectorContainer;opener.parentNode.className='selected';var right=container.parentNode.offsetWidth-(opener.offsetLeft+opener.offsetWidth);container.style.display='block';container.style.right=right+'px';if(shouldFocus)
this._paneSelector.focus();}
_closePaneSelector()
{this._paneSelectorOpener.parentNode.className='';this._paneSelectorContainer.style.display='none';}
_addPane(platform,metric)
{if(this._addPaneCallback)
this._addPaneCallback(platform,metric);}
static htmlTemplate()
{return` <nav class="charts-toolbar"> <ul class="buttoned-toolbar"> <li><a href="#" class="pane-selector-opener">Add pane</a></li> </ul> <ul class="buttoned-toolbar"> <li class="start-time-slider"> <label> <input class="slider" type="range"> <input class="editor" type="number"> <span class="label"><span class="day-count" tabindex="0">?</span> days</span> </label> </li> </ul> <div class="pane-selector-container"> <pane-selector></pane-selector> </div> </nav>`;}
static cssTemplate()
{return Toolbar.cssTemplate()+` .charts-toolbar>.buttoned-toolbar:first-child{margin-right:0.5rem}.buttoned-toolbar li a.pane-selector-opener:hover{background:rgba(204,153,51,0.1)}.charts-toolbar>.pane-selector-container{position:absolute;right:1rem;margin:0;margin-top:-0.2rem;margin-right:-0.5rem;padding:1rem;border:solid 1px #ccc;border-radius:0.2rem;background:rgba(255,255,255,0.8);-webkit-backdrop-filter:blur(0.5rem)}.buttoned-toolbar .start-time-slider{margin-left:2rem;line-height:1em;font-size:0.9rem}.start-time-slider label{display:inline-block}.start-time-slider .slider{height:0.8rem}.start-time-slider .editor{position:absolute;opacity:0;width:4rem;font-weight:inherit;font-size:0.8rem;height:0.9rem;outline:none;border:solid 1px #ccc;z-index:5}.start-time-slider .day-count{display:inline-block;text-align:right;width:2rem}`;}}
class ChartsPage extends PageWithHeading{constructor(toolbar)
{console.assert(toolbar instanceof ChartsToolbar);super('Charts',toolbar);this._paneList=[];this._paneListChanged=false;this._mainDomain=null;this._currentRepositoryId=null;toolbar.setAddPaneCallback(this.insertPaneAfter.bind(this));}
routeName(){return'charts';}
static createStateForDashboardItem(platformId,metricId,startTime)
{var state={paneList:[[platformId,metricId]],since:startTime};return state;}
static createDomainForAnalysisTask(task)
{var diff=(task.endTime()-task.startTime())*0.1;return[task.startTime()-diff,task.endTime()+diff];}
static createStateForAnalysisTask(task)
{var domain=this.createDomainForAnalysisTask(task);var state={paneList:[[task.platform().id(),task.metric().id()]],since:Math.round(task.startTime()-(Date.now()-task.startTime())*0.1),zoom:domain,};return state;}
static createStateForConfigurationList(configurationList,startTime)
{console.assert(configurationList instanceof Array);var state={paneList:configurationList};return state;}
open(state)
{this.toolbar().setNumberOfDaysCallback(this.setNumberOfDaysFromToolbar.bind(this));super.open(state);}
serializeState()
{var state={since:this.toolbar().startTime()};var serializedPaneList=[];for(var pane of this._paneList)
serializedPaneList.push(pane.serializeState());if(this._mainDomain)
state['zoom']=this._mainDomain;if(serializedPaneList.length)
state['paneList']=serializedPaneList;var repository=this._currentRepositoryId;if(repository)
state['repository']=this._currentRepositoryId;return state;}
updateFromSerializedState(state,isOpen)
{var paneList=[];if(state.paneList instanceof Array)
paneList=state.paneList;var newPaneList=this._updateChartPanesFromSerializedState(paneList);if(newPaneList){this._paneList=newPaneList;this._paneListChanged=true;this.enqueueToRender();}
this._updateDomainsFromSerializedState(state);this._currentRepositoryId=parseInt(state['repository']);var currentRepository=Repository.findById(this._currentRepositoryId);console.assert(this._paneList.length==paneList.length);for(var i=0;i<this._paneList.length;i++){this._paneList[i].updateFromSerializedState(state.paneList[i],isOpen);this._paneList[i].setOpenRepository(currentRepository);}}
_updateDomainsFromSerializedState(state)
{var since=parseFloat(state.since);var zoom=state.zoom;if(typeof(zoom)=='string'&&zoom.indexOf('-'))
zoom=zoom.split('-')
if(zoom instanceof Array&&zoom.length>=2)
zoom=zoom.map(function(value){return parseFloat(value);});if(zoom&&since)
since=Math.min(zoom[0],since);else if(zoom)
since=zoom[0]-(zoom[1]-zoom[0])/2;this.toolbar().setStartTime(since);this.toolbar().enqueueToRender();this._mainDomain=zoom||null;this._updateOverviewDomain();this._updateMainDomain();}
_updateChartPanesFromSerializedState(paneList)
{var paneMap={}
for(var pane of this._paneList)
paneMap[pane.platformId()+'-'+pane.metricId()]=pane;var newPaneList=[];var createdNewPane=false;for(var paneInfo of paneList){var platformId=parseInt(paneInfo[0]);var metricId=parseInt(paneInfo[1]);var existingPane=paneMap[platformId+'-'+metricId];if(existingPane)
newPaneList.push(existingPane);else{newPaneList.push(new ChartPane(this,platformId,metricId));createdNewPane=true;}}
if(createdNewPane||newPaneList.length!==this._paneList.length)
return newPaneList;for(var i=0;i<newPaneList.length;i++){if(newPaneList[i]!=this._paneList[i])
return newPaneList;}
return null;}
setNumberOfDaysFromToolbar(numberOfDays,shouldUpdateState)
{this.toolbar().setNumberOfDays(numberOfDays,true);this.toolbar().enqueueToRender();this._updateOverviewDomain();this._updateMainDomain();if(shouldUpdateState)
this.scheduleUrlStateUpdate();}
setMainDomainFromOverviewSelection(domain,originatingPane,shouldUpdateState)
{this._mainDomain=domain;this._updateMainDomain(originatingPane);if(shouldUpdateState)
this.scheduleUrlStateUpdate();}
setMainDomainFromZoom(selection,originatingPane)
{this._mainDomain=selection;this._updateMainDomain(originatingPane);this.scheduleUrlStateUpdate();}
mainChartSelectionDidChange(pane,shouldUpdateState)
{if(shouldUpdateState)
this.scheduleUrlStateUpdate();}
mainChartIndicatorDidChange(pane,shouldUpdateState)
{if(shouldUpdateState)
this.scheduleUrlStateUpdate();}
graphOptionsDidChange(pane)
{this.scheduleUrlStateUpdate();}
setOpenRepository(repository)
{this._currentRepositoryId=repository?repository.id():null;for(var pane of this._paneList)
pane.setOpenRepository(repository);this.scheduleUrlStateUpdate();}
_updateOverviewDomain()
{var startTime=this.toolbar().startTime();var endTime=this.toolbar().endTime();for(var pane of this._paneList)
pane.setOverviewDomain(startTime,endTime);}
_updateMainDomain(originatingPane)
{var startTime=this.toolbar().startTime();var endTime=this.toolbar().endTime();if(this._mainDomain){startTime=this._mainDomain[0];endTime=this._mainDomain[1];}
for(var pane of this._paneList){pane.setMainDomain(startTime,endTime);if(pane!=originatingPane)
pane.setOverviewSelection(this._mainDomain);}}
closePane(pane)
{var index=this._paneList.indexOf(pane);console.assert(index>=0);this._paneList.splice(index,1);this._didMutatePaneList(false);}
insertPaneAfter(platform,metric,referencePane)
{var newPane=new ChartPane(this,platform.id(),metric.id());if(referencePane){var index=this._paneList.indexOf(referencePane);console.assert(index>=0);this._paneList.splice(index+1,0,newPane);}else
this._paneList.unshift(newPane);this._didMutatePaneList(true);}
alternatePlatforms(platform,metric)
{var existingPlatforms={};for(var pane of this._paneList){if(pane.metricId()==metric.id())
existingPlatforms[pane.platformId()]=true;}
return metric.platforms().filter(function(platform){return!existingPlatforms[platform.id()]&&!platform.isHidden();});}
insertBreakdownPanesAfter(platform,metric,referencePane)
{console.assert(referencePane);var childMetrics=metric.childMetrics().filter(metric=>!metric.test().isHidden());var index=this._paneList.indexOf(referencePane);console.assert(index>=0);var args=[index+1,0];for(var metric of childMetrics)
args.push(new ChartPane(this,platform.id(),metric.id()));this._paneList.splice.apply(this._paneList,args);this._didMutatePaneList(true);}
canBreakdown(platform,metric)
{var childMetrics=metric.childMetrics().filter(metric=>!metric.test().isHidden());if(!childMetrics.length)
return false;var existingMetrics={};for(var pane of this._paneList){if(pane.platformId()==platform.id())
existingMetrics[pane.metricId()]=true;}
for(var metric of childMetrics){if(!existingMetrics[metric.id()])
return true;}
return false;}
_didMutatePaneList(addedNewPane)
{this._paneListChanged=true;if(addedNewPane){this._updateOverviewDomain();this._updateMainDomain();}
this.enqueueToRender();this.scheduleUrlStateUpdate();}
render()
{super.render();if(this._paneListChanged)
this.renderReplace(this.content().querySelector('.pane-list'),this._paneList);for(var pane of this._paneList)
pane.enqueueToRender();this._paneListChanged=false;}
static htmlTemplate()
{return`<div class="pane-list"></div>`;}}
class AnalysisCategoryToolbar extends ComponentBase{constructor(categoryPage)
{super('analysis-category-toolbar');this._categoryPage=null;this._categories=AnalysisTask.categories();this._currentCategory=null;this._filter=null;this.setCategoryIfValid(null);this._filterInput=this.content().querySelector('input');this._filterInput.oninput=this._filterMayHaveChanged.bind(this,false);this._filterInput.onchange=this._filterMayHaveChanged.bind(this,true);}
setCategoryPage(categoryPage){this._categoryPage=categoryPage;}
currentCategory(){return this._currentCategory;}
filter(){return this._filter;}
setFilter(filter){this._filter=filter;}
render()
{if(!this._categoryPage)
return;var router=this._categoryPage.router();console.assert(router);super.render();var element=ComponentBase.createElement;var link=ComponentBase.createLink;if(this._filterInput.value!=this._filter)
this._filterInput.value=this._filter;var currentCategory=this._currentCategory;var categoryPage=this._categoryPage;this.renderReplace(this.content().querySelector('.analysis-task-category-toolbar'),this._categories.map(function(category){return element('li',{class:category==currentCategory?'selected':null},link(category,router.url(categoryPage.routeName(),categoryPage.stateForCategory(category))));}));}
_filterMayHaveChanged(shouldUpdateState,event)
{var input=event.target;var oldFilter=this._filter;this._filter=input.value;if(this._filter!=oldFilter&&this._categoryPage||shouldUpdateState)
this._categoryPage.filterDidChange(shouldUpdateState);}
setCategoryIfValid(category)
{if(!category)
category=this._categories[0];if(this._categories.indexOf(category)<0)
return false;this._currentCategory=category;return true;}
static cssTemplate()
{return Toolbar.cssTemplate()+` .queue-toolbar{position:absolute;right:1rem}.trybot-button{position:absolute;left:1rem}`}
static htmlTemplate()
{return` <ul class="buttoned-toolbar trybot-button"> <li><a href="#/analysis/task/create">Create</a></li> </ul> <ul class="analysis-task-category-toolbar buttoned-toolbar"></ul> <input type="text"> <ul class="buttoned-toolbar queue-toolbar"> <li><a href="#/analysis/queue">Queue</a></li> </ul>`;}}
ComponentBase.defineElement('analysis-category-toolbar',AnalysisCategoryToolbar);class AnalysisCategoryPage extends PageWithHeading{constructor()
{super('Analysis');this._categoryToolbar=this.content().querySelector('analysis-category-toolbar').component();this._categoryToolbar.setCategoryPage(this);this._renderedList=false;this._renderedFilter=false;this._fetched=false;this._errorMessage=null;}
title()
{var category=this._categoryToolbar.currentCategory();return(category?category.charAt(0).toUpperCase()+category.slice(1)+' ':'')+'Analysis Tasks';}
routeName(){return'analysis';}
open(state)
{var self=this;AnalysisTask.fetchAll().then(function(){self._fetched=true;self.enqueueToRender();},function(error){self._errorMessage='Failed to fetch the list of analysis tasks: '+error;self.enqueueToRender();});super.open(state);}
serializeState()
{return this.stateForCategory(this._categoryToolbar.currentCategory());}
stateForCategory(category)
{var state={category:category};var filter=this._categoryToolbar.filter();if(filter)
state.filter=filter;return state;}
updateFromSerializedState(state,isOpen)
{if(state.category instanceof Set)
state.category=Array.from(state.category.values())[0];if(state.filter instanceof Set)
state.filter=Array.from(state.filter.values())[0];if(this._categoryToolbar.setCategoryIfValid(state.category))
this._renderedList=false;if(state.filter)
this._categoryToolbar.setFilter(state.filter);if(!isOpen)
this.enqueueToRender();}
filterDidChange(shouldUpdateState)
{this.enqueueToRender();if(shouldUpdateState)
this.scheduleUrlStateUpdate();}
render()
{Instrumentation.startMeasuringTime('AnalysisCategoryPage','render');super.render();this._categoryToolbar.enqueueToRender();if(this._errorMessage){console.assert(!this._fetched);var element=ComponentBase.createElement;this.renderReplace(this.content().querySelector('tbody.analysis-tasks'),element('tr',element('td',{colspan:6},this._errorMessage)));this._renderedList=true;return;}
if(!this._fetched)
return;if(!this._renderedList){this._reconstructTaskList();this._renderedList=true;}
var filter=this._categoryToolbar.filter();if(filter||this._renderedFilter){Instrumentation.startMeasuringTime('AnalysisCategoryPage','filterByKeywords');var keywordList=filter?filter.toLowerCase().split(/\s+/):[];var tableRows=this.content().querySelectorAll('tbody.analysis-tasks tr');for(var i=0;i<tableRows.length;i++){var row=tableRows[i];var textContent=row.textContent.toLowerCase();var display=null;for(var keyword of keywordList){if(textContent.indexOf(keyword)<0){display='none';break;}}
row.style.display=display;}
this._renderedFilter=!!filter;Instrumentation.endMeasuringTime('AnalysisCategoryPage','filterByKeywords');}
Instrumentation.endMeasuringTime('AnalysisCategoryPage','render');}
_reconstructTaskList()
{Instrumentation.startMeasuringTime('AnalysisCategoryPage','reconstructTaskList');console.assert(this.router());const currentCategory=this._categoryToolbar.currentCategory();const tasks=AnalysisTask.all().filter((task)=>task.category()==currentCategory).sort((a,b)=>{if(a.hasPendingRequests()==b.hasPendingRequests())
return b.createdAt()-a.createdAt();else if(a.hasPendingRequests()) 
return-1;else if(b.hasPendingRequests()) 
return 1;return 0;});const element=ComponentBase.createElement;const link=ComponentBase.createLink;const router=this.router();this.renderReplace(this.content().querySelector('tbody.analysis-tasks'),tasks.map((task)=>{const status=AnalysisCategoryPage._computeStatus(task);return element('tr',[element('td',{class:'status'},element('span',{class:status.class},status.label)),element('td',link(task.label(),router.url(`analysis/task/${task.id()}`))),element('td',{class:'bugs'},element('ul',task.bugs().map((bug)=>{const url=bug.url();const title=bug.title();return element('li',url?link(bug.label(),title,url,true):title);}))),element('td',{class:'author'},task.author()),element('td',{class:'platform'},task.platform()?task.platform().label():null),element('td',task.metric()?task.metric().fullName():null),]);}));Instrumentation.endMeasuringTime('AnalysisCategoryPage','reconstructTaskList');}
static _computeStatus(task)
{if(task.hasPendingRequests())
return{label:task.requestLabel(),class:'bisecting'};var type=task.changeType();switch(type){case'regression':return{label:'Regression',class:type};case'progression':return{label:'Progression',class:type};case'unchanged':return{label:'No change',class:type};case'inconclusive':return{label:'Inconclusive',class:type};}
if(task.hasResults())
return{label:'New results',class:'bisecting'};return{label:'Unconfirmed',class:'unconfirmed'};}
static htmlTemplate()
{return` <div class="toolbar-container"><analysis-category-toolbar></analysis-category-toolbar></div> <div class="analysis-task-category"> <table> <thead> <tr> <td class="status">Status</td> <td>Name</td> <td>Bugs</td> <td>Author</td> <td>Platform</td> <td>Test Metric</td> </tr> </thead> <tbody class="analysis-tasks"></tbody> </table> </div>`;}
static cssTemplate()
{return` .toolbar-container{text-align:center}.analysis-task-category{width:calc(100% - 2rem);margin:1rem}.analysis-task-category table{width:100%;border:0;border-collapse:collapse}.analysis-task-category td,.analysis-task-category th{border:none;border-collapse:collapse;text-align:left;font-size:0.9rem;font-weight:normal}.analysis-task-category thead td{color:#f96;font-weight:inherit;font-size:1.1rem;padding:0.2rem 0.4rem}.analysis-task-category tbody td{border-top:solid 1px #eee;border-bottom:solid 1px #eee;padding:0.2rem 0.2rem}.analysis-task-category .bugs ul,.analysis-task-category .bugs li{padding:0;margin:0;list-style:none}.analysis-task-category .status,.analysis-task-category .author,.analysis-task-category .platform{text-align:center}.analysis-task-category .status span{display:inline;white-space:nowrap;border-radius:0.3rem;padding:0.2rem 0.3rem;font-size:0.8rem}.analysis-task-category .status .regression{background:#c33;color:#fff}.analysis-task-category .status .progression{background:#36f;color:#fff}.analysis-task-category .status .unchanged{background:#ccc;color:white}.analysis-task-category .status .inconclusive{background:#666;color:white}.analysis-task-category .status .bisecting{background:#ff9}.analysis-task-category .status .unconfirmed{background:#f96}`;}}
class AnalysisTaskChartPane extends ChartPaneBase{constructor()
{super('analysis-task-chart-pane');this._page=null;this._showForm=false;}
setPage(page){this._page=page;}
setShowForm(show)
{this._showForm=show;this.enqueueToRender();}
router(){return this._page.router();}
_mainSelectionDidChange(selection,didEndDrag)
{super._mainSelectionDidChange(selection);if(didEndDrag)
this.enqueueToRender();}
didConstructShadowTree()
{this.part('form').listenToAction('startTesting',(name,repetitionCount,repetitionType,commitSetMap,notifyOnCompletion)=>{this.dispatchAction('newTestGroup',name,repetitionCount,repetitionType,commitSetMap,notifyOnCompletion);});}
render()
{super.render();const points=this._mainChart?this._mainChart.selectedPoints('current'):null;this.content('form').style.display=this._showForm?null:'none';if(this._showForm){const form=this.part('form');form.setCommitSetMap(points&&points.length()>=2?{'A':points.firstPoint().commitSet(),'B':points.lastPoint().commitSet()}:null);form.enqueueToRender();}}
static paneFooterTemplate(){return'<customizable-test-group-form id="form"></customizable-test-group-form>';}
static cssTemplate()
{return super.cssTemplate()+` #form { margin: 0.5rem; } `;}}
ComponentBase.defineElement('analysis-task-chart-pane',AnalysisTaskChartPane);class AnalysisTaskResultsPane extends ComponentBase{constructor()
{super('analysis-task-results-pane');this._showForm=false;this._repositoryList=[];this._renderRepositoryListLazily=new LazilyEvaluatedFunction(this._renderRepositoryList.bind(this));this._updateCommitViewerLazily=new LazilyEvaluatedFunction(this._updateCommitViewer.bind(this));}
setPoints(startPoint,endPoint,metric)
{const resultsViewer=this.part('results-viewer');this._repositoryList=startPoint?Repository.sortByNamePreferringOnesWithURL(startPoint.commitSet().repositories()):[];resultsViewer.setPoints(startPoint,endPoint,metric);resultsViewer.enqueueToRender();}
setTestGroups(testGroups,currentGroup)
{this.part('results-viewer').setTestGroups(testGroups,currentGroup);if(currentGroup)
this.part('form').updateWithTestGroup(currentGroup);this.enqueueToRender();}
setAnalysisResultsView(analysisResultsView)
{this.part('results-viewer').setAnalysisResultsView(analysisResultsView);this.enqueueToRender();}
setShowForm(show)
{this._showForm=show;this.enqueueToRender();}
didConstructShadowTree()
{const resultsViewer=this.part('results-viewer');resultsViewer.listenToAction('testGroupClick',(testGroup)=>{this.enqueueToRender();this.dispatchAction('showTestGroup',testGroup)});resultsViewer.setRangeSelectorLabels(['A','B']);resultsViewer.listenToAction('rangeSelectorClick',()=>this.enqueueToRender());const repositoryPicker=this.content('commit-viewer-repository');repositoryPicker.addEventListener('change',()=>this.enqueueToRender());this.part('commit-viewer').setShowRepositoryName(false);this.part('form').listenToAction('startTesting',(name,repetitionCount,repetitionType,commitSetMap,notifyOnCompletion)=>{this.dispatchAction('newTestGroup',name,repetitionCount,repetitionType,commitSetMap,notifyOnCompletion);});}
render()
{const resultsViewer=this.part('results-viewer');const repositoryPicker=this._renderRepositoryListLazily.evaluate(this._repositoryList);const repository=Repository.findById(repositoryPicker.value);const range=resultsViewer.selectedRange();this._updateCommitViewerLazily.evaluate(repository,range['A'],range['B']);this.content('form').style.display=this._showForm?null:'none';if(!this._showForm)
return;const selectedRange=this.part('results-viewer').selectedRange();const firstCommitSet=selectedRange['A'];const secondCommitSet=selectedRange['B'];const form=this.part('form');form.setCommitSetMap(firstCommitSet&&secondCommitSet?{'A':firstCommitSet,'B':secondCommitSet}:null);form.enqueueToRender();}
_renderRepositoryList(repositoryList)
{const element=ComponentBase.createElement;const selectElement=this.content('commit-viewer-repository');this.renderReplace(selectElement,repositoryList.map((repository)=>{return element('option',{value:repository.id()},repository.label());}));return selectElement;}
_updateCommitViewer(repository,preceedingCommitSet,lastCommitSet)
{if(repository&&preceedingCommitSet&&lastCommitSet&&!preceedingCommitSet.equals(lastCommitSet)){const precedingRevision=preceedingCommitSet.revisionForRepository(repository);const lastRevision=lastCommitSet.revisionForRepository(repository);if(precedingRevision&&lastRevision&&precedingRevision!=lastRevision){this.part('commit-viewer').view(repository,precedingRevision,lastRevision);return;}}
this.part('commit-viewer').view(null,null,null);}
static htmlTemplate()
{return` <div id="results-container"> <div id="results-viewer-container"> <analysis-results-viewer id="results-viewer"></analysis-results-viewer> </div> <div id="commit-pane"> <select id="commit-viewer-repository"></select> <commit-log-viewer id="commit-viewer"></commit-log-viewer> </div> </div> <customizable-test-group-form id="form"></customizable-test-group-form> `;}
static cssTemplate()
{return` #results-container{position:relative;text-align:center;padding-right:20rem}#results-viewer-container{overflow-x:scroll;overflow-y:hidden}#commit-pane{position:absolute;width:20rem;height:100%;top:0;right:0}#form{margin:0.5rem}`;}}
ComponentBase.defineElement('analysis-task-results-pane',AnalysisTaskResultsPane);class AnalysisTaskConfiguratorPane extends ComponentBase{constructor()
{super('analysis-task-configurator-pane');this._currentGroup=null;}
didConstructShadowTree()
{const form=this.part('form');form.setHasTask(true);form.listenToAction('startTesting',(...args)=>{this.dispatchAction('createCustomTestGroup',...args);});}
setTestGroups(testGroups,currentGroup)
{this._currentGroup=currentGroup;const form=this.part('form');if(!form.hasCommitSets()&&currentGroup)
form.setConfigurations(currentGroup.test(),currentGroup.platform(),currentGroup.initialRepetitionCount(),currentGroup.requestedCommitSets());this.enqueueToRender();}
render()
{super.render();}
static htmlTemplate()
{return`<custom-configuration-test-group-form id="form"></custom-configuration-test-group-form>`;}
static cssTemplate()
{return` #form{margin:1rem}`;}}
ComponentBase.defineElement('analysis-task-configurator-pane',AnalysisTaskConfiguratorPane);class AnalysisTaskTestGroupPane extends ComponentBase{constructor()
{super('analysis-task-test-group-pane');this._renderTestGroupsLazily=new LazilyEvaluatedFunction(this._renderTestGroups.bind(this));this._renderTestGroupVisibilityLazily=new LazilyEvaluatedFunction(this._renderTestGroupVisibility.bind(this));this._renderTestGroupNamesLazily=new LazilyEvaluatedFunction(this._renderTestGroupNames.bind(this));this._renderCurrentTestGroupLazily=new LazilyEvaluatedFunction(this._renderCurrentTestGroup.bind(this));this._testGroupMap=new Map;this._testGroups=[];this._bisectingCommitSetByTestGroup=null;this._currentTestGroup=null;this._showHiddenGroups=false;this._allTestGroupIdSetForCurrentTask=null;}
didConstructShadowTree()
{this.content('hide-button').onclick=()=>this.dispatchAction('toggleTestGroupVisibility',this._currentTestGroup);this.part('retry-form').listenToAction('startTesting',(repetitionCount,repetitionType,notifyOnCompletion)=>{this.dispatchAction('retryTestGroup',this._currentTestGroup,repetitionCount,repetitionType,notifyOnCompletion);});this.part('bisect-form').listenToAction('startTesting',(repetitionCount,repetitionType,notifyOnCompletion)=>{const bisectingCommitSet=this._bisectingCommitSetByTestGroup.get(this._currentTestGroup);const[oneCommitSet,anotherCommitSet]=this._currentTestGroup.requestedCommitSets();const commitSets=[oneCommitSet,bisectingCommitSet,anotherCommitSet];this.dispatchAction('bisectTestGroup',this._currentTestGroup,commitSets,repetitionCount,repetitionType,notifyOnCompletion);});}
setTestGroups(testGroups,currentTestGroup,showHiddenGroups)
{this._testGroups=testGroups;this._currentTestGroup=currentTestGroup;this._showHiddenGroups=showHiddenGroups;this.part('revision-table').setTestGroup(currentTestGroup);this.part('results-viewer').setTestGroup(currentTestGroup);const analysisTask=currentTestGroup.task();const allTestGroupIdsForCurrentTask=TestGroup.findAllByTask(analysisTask.id()).map((testGroup)=>testGroup.id());const testGroupChanged=!this._allTestGroupIdSetForCurrentTask||this._allTestGroupIdSetForCurrentTask.size!==allTestGroupIdsForCurrentTask.length||!allTestGroupIdsForCurrentTask.every((testGroupId)=>this._allTestGroupIdSetForCurrentTask.has(testGroupId));const computedForCurrentTestGroup=this._bisectingCommitSetByTestGroup&&this._bisectingCommitSetByTestGroup.has(currentTestGroup);if(!testGroupChanged&&computedForCurrentTestGroup){this.enqueueToRender();return;}
if(testGroupChanged){this._bisectingCommitSetByTestGroup=new Map;this._allTestGroupIdSetForCurrentTask=new Set(allTestGroupIdsForCurrentTask);}
analysisTask.commitSetsFromTestGroupsAndMeasurementSet().then(async(availableCommitSets)=>{const commitSetClosestToMiddle=await CommitSetRangeBisector.commitSetClosestToMiddleOfAllCommits(currentTestGroup.requestedCommitSets(),availableCommitSets);this._bisectingCommitSetByTestGroup.set(currentTestGroup,commitSetClosestToMiddle);this.enqueueToRender();});}
setAnalysisResults(analysisResults,metric)
{this.part('revision-table').setAnalysisResults(analysisResults);this.part('results-viewer').setAnalysisResults(analysisResults,metric);this.enqueueToRender();}
render()
{this._renderTestGroupsLazily.evaluate(this._showHiddenGroups,...this._testGroups);this._renderTestGroupVisibilityLazily.evaluate(...this._testGroups.map((group)=>group.isHidden()?'hidden':'visible'));this._renderTestGroupNamesLazily.evaluate(...this._testGroups.map((group)=>group.label()));this._renderCurrentTestGroup(this._currentTestGroup);}
_renderTestGroups(showHiddenGroups,...testGroups)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;this._testGroupMap=new Map;const testGroupItems=testGroups.map((group)=>{const text=new EditableText(group.label());text.listenToAction('update',()=>this.dispatchAction('renameTestGroup',group,text.editedText()));const listItem=element('li',link(text,group.label(),()=>this.dispatchAction('showTestGroup',group)));this._testGroupMap.set(group,{text,listItem});return listItem;});this.renderReplace(this.content('test-group-list'),[testGroupItems,showHiddenGroups?[]:element('li',{class:'test-group-list-show-all'},link('Show hidden tests',()=>{this.dispatchAction('showHiddenTestGroups');}))]);}
_renderTestGroupVisibility(...groupVisibilities)
{for(let i=0;i<groupVisibilities.length;i++)
this._testGroupMap.get(this._testGroups[i]).listItem.className=groupVisibilities[i];}
_renderTestGroupNames(...groupNames)
{for(let i=0;i<groupNames.length;i++)
this._testGroupMap.get(this._testGroups[i]).text.setText(groupNames[i]);}
_renderCurrentTestGroup(currentGroup)
{const selected=this.content('test-group-list').querySelector('.selected');if(selected)
selected.classList.remove('selected');if(currentGroup){this.part('retry-form').updateWithTestGroup(currentGroup);this.part('bisect-form').updateWithTestGroup(currentGroup);this._testGroupMap.get(currentGroup).listItem.classList.add('selected');if(currentGroup.hasRetries())
this.renderReplace(this.content('retry-summary'),this._retrySummary(currentGroup));const authoredBy=currentGroup.author()?`by "${currentGroup.author()}"`:'';this.renderReplace(this.content('request-summary'),`Scheduled ${authoredBy} at ${currentGroup.createdAt()}`)
this.renderReplace(this.content('hide-button'),currentGroup.isHidden()?'Unhide':'Hide');this.renderReplace(this.content('pending-request-cancel-warning'),currentGroup.hasPending()?'(cancels pending requests)':[]);}
this.content('retry-form').style.display=currentGroup?null:'none';this.content('bisect-form').style.display=currentGroup&&this._bisectingCommitSetByTestGroup.get(currentGroup)?null:'none';}
_retrySummary(testGroup)
{if(!testGroup.hasRetries())
return'';if(testGroup.retryCountsAreSameForAllCommitSets()){const retryCount=testGroup.retryCountForCommitSet(testGroup.requestedCommitSets()[0]);return`${testGroup.initialRepetitionCount()} requested, ${retryCount} added due to failures.`;}
const retrySummaryParts=testGroup.requestedCommitSets().map((commitSet)=>({commitSet,retryCount:testGroup.retryCountForCommitSet(commitSet)})).filter((item)=>item.retryCount).map((item)=>`${item.retryCount} added to ${testGroup.labelForCommitSet(item.commitSet)}`);const retrySummary=new Intl.ListFormat('en',{style:'long',type:'conjunction'}).format(retrySummaryParts);return`${testGroup.initialRepetitionCount()} requested, ${retrySummary} due to failures.`;}
static htmlTemplate()
{return` <ul id="test-group-list"></ul> <div id="test-group-details"> <test-group-results-viewer id="results-viewer"></test-group-results-viewer> <test-group-revision-table id="revision-table"></test-group-revision-table> <div id="retry-summary" class="summary"></div> <div id="request-summary" class="summary"></div> <test-group-form id="retry-form">Retry</test-group-form> <test-group-form id="bisect-form">Bisect</test-group-form> <button id="hide-button">Hide</button> <span id="pending-request-cancel-warning">(cancels pending requests)</span> </div>`;}
static cssTemplate()
{return` :host{display:flex!important;font-size:0.9rem}#test-group-list{flex:none;margin:0;padding:0.2rem 0;list-style:none;border-right:solid 1px #ccc;white-space:nowrap;min-width:8rem}li{display:block;font-size:0.9rem}li>a{display:block;color:inherit;text-decoration:none;margin:0;padding:0.2rem}li.test-group-list-show-all{font-size:0.8rem;margin-top:0.5rem;padding-right:1rem;text-align:center;color:#999}li.test-group-list-show-all:not(.selected) a:hover{background:inherit}li.selected>a{background:rgba(204,153,51,0.1)}li.hidden{color:#999}li:not(.selected)>a:hover{background:#eee}div.summary{padding-left:1rem}#test-group-details{display:table-cell;margin-bottom:1rem;padding:0 0.5rem;margin:0}#retry-form,#bisect-form{display:block;margin:0.5rem}#hide-button{margin:0.5rem}`;}}
ComponentBase.defineElement('analysis-task-test-group-pane',AnalysisTaskTestGroupPane);class AnalysisTaskPage extends PageWithHeading{constructor()
{super('Analysis Task');this._renderTaskNameAndStatusLazily=new LazilyEvaluatedFunction(this._renderTaskNameAndStatus.bind(this));this._renderCauseAndFixesLazily=new LazilyEvaluatedFunction(this._renderCauseAndFixes.bind(this));this._renderRelatedTasksLazily=new LazilyEvaluatedFunction(this._renderRelatedTasks.bind(this));this._resetVariables();}
title(){return this._task?this._task.label():'Analysis Task';}
routeName(){return'analysis/task';}
_resetVariables()
{this._task=null;this._metric=null;this._triggerable=null;this._relatedTasks=null;this._testGroups=null;this._testGroupLabelMap=new Map;this._analysisResults=null;this._measurementSet=null;this._startPoint=null;this._endPoint=null;this._errorMessage=null;this._currentTestGroup=null;this._filteredTestGroups=null;this._showHiddenTestGroups=false;}
updateFromSerializedState(state)
{this._resetVariables();if(state.remainingRoute){const taskId=parseInt(state.remainingRoute);AnalysisTask.fetchById(taskId).then(this._didFetchTask.bind(this)).then(()=>{this._fetchRelatedInfoForTaskId(taskId);},(error)=>{this._errorMessage=`Failed to fetch the analysis task ${state.remainingRoute}: ${error}`;this.enqueueToRender();});}else if(state.buildRequest){const buildRequestId=parseInt(state.buildRequest);AnalysisTask.fetchByBuildRequestId(buildRequestId).then(this._didFetchTask.bind(this)).then((task)=>{this._fetchRelatedInfoForTaskId(task.id());},(error)=>{this._errorMessage=`Failed to fetch the analysis task for the build request ${buildRequestId}: ${error}`;this.enqueueToRender();});}}
didConstructShadowTree()
{this.part('analysis-task-name').listenToAction('update',()=>this._updateTaskName(this.part('analysis-task-name').editedText()));this.content('change-type-form').onsubmit=ComponentBase.createEventHandler((event)=>this._updateChangeType(event));this.part('chart-pane').listenToAction('newTestGroup',this._createTestGroupAfterVerifyingCommitSetList.bind(this));const resultsPane=this.part('results-pane');resultsPane.listenToAction('showTestGroup',(testGroup)=>this._showTestGroup(testGroup));resultsPane.listenToAction('newTestGroup',this._createTestGroupAfterVerifyingCommitSetList.bind(this));this.part('configurator-pane').listenToAction('createCustomTestGroup',this._createCustomTestGroup.bind(this));const groupPane=this.part('group-pane');groupPane.listenToAction('showTestGroup',(testGroup)=>this._showTestGroup(testGroup));groupPane.listenToAction('showHiddenTestGroups',()=>this._showAllTestGroups());groupPane.listenToAction('renameTestGroup',(testGroup,newName)=>this._updateTestGroupName(testGroup,newName));groupPane.listenToAction('toggleTestGroupVisibility',(testGroup)=>this._hideCurrentTestGroup(testGroup));groupPane.listenToAction('retryTestGroup',(testGroup,repetitionCount,repetitionType,notifyOnCompletion)=>this._retryCurrentTestGroup(testGroup,repetitionCount,repetitionType,notifyOnCompletion));groupPane.listenToAction('bisectTestGroup',(testGroup,commitSets,repetitionCount,repetitionType,notifyOnCompletion)=>this._bisectCurrentTestGroup(testGroup,commitSets,repetitionCount,repetitionType,notifyOnCompletion));this.part('cause-list').listenToAction('addItem',(repository,revision)=>{this._associateCommit('cause',repository,revision);});this.part('fix-list').listenToAction('addItem',(repository,revision)=>{this._associateCommit('fix',repository,revision);});}
_fetchRelatedInfoForTaskId(taskId)
{TestGroup.fetchForTask(taskId).then(this._didFetchTestGroups.bind(this));AnalysisResults.fetch(taskId).then(this._didFetchAnalysisResults.bind(this));AnalysisTask.fetchRelatedTasks(taskId).then((relatedTasks)=>{this._relatedTasks=relatedTasks;this.enqueueToRender();});}
_didFetchTask(task)
{console.assert(!this._task);this._task=task;const bugList=this.part('bug-list');this.part('bug-list').setTask(this._task);this.enqueueToRender();if(task.isCustom())
return task;const platform=task.platform();const metric=task.metric();const lastModified=platform.lastModified(metric);const triggerable=Triggerable.findByTestConfiguration(metric.test(),platform);this._triggerable=triggerable&&!triggerable.isDisabled()?triggerable:null;this._metric=metric;this._measurementSet=MeasurementSet.findSet(platform.id(),metric.id(),lastModified);this._measurementSet.fetchBetween(task.startTime(),task.endTime(),this._didFetchMeasurement.bind(this));const chart=this.part('chart-pane');const domain=ChartsPage.createDomainForAnalysisTask(task);chart.configure(platform.id(),metric.id());chart.setOverviewDomain(domain[0],domain[1]);chart.setMainDomain(domain[0],domain[1]);return task;}
_didFetchMeasurement()
{console.assert(this._task);console.assert(this._measurementSet);var series=this._measurementSet.fetchedTimeSeries('current',false,false);var startPoint=series.findById(this._task.startMeasurementId());var endPoint=series.findById(this._task.endMeasurementId());if(!startPoint||!endPoint||!this._measurementSet.hasFetchedRange(startPoint.time,endPoint.time))
return;this.part('results-pane').setPoints(startPoint,endPoint,this._task.metric());this._startPoint=startPoint;this._endPoint=endPoint;this.enqueueToRender();}
_didFetchTestGroups(testGroups)
{this._testGroups=testGroups.sort(function(a,b){return+a.createdAt()-b.createdAt();});this._didUpdateTestGroupHiddenState();this._assignTestResultsIfPossible();this.enqueueToRender();}
_showAllTestGroups()
{this._showHiddenTestGroups=true;this._didUpdateTestGroupHiddenState();this.enqueueToRender();}
_didUpdateTestGroupHiddenState()
{if(!this._showHiddenTestGroups)
this._filteredTestGroups=this._testGroups.filter(function(group){return!group.isHidden();});else
this._filteredTestGroups=this._testGroups;this._showTestGroup(this._filteredTestGroups?this._filteredTestGroups[this._filteredTestGroups.length-1]:null);}
_didFetchAnalysisResults(results)
{this._analysisResults=results;if(this._assignTestResultsIfPossible())
this.enqueueToRender();}
_assignTestResultsIfPossible()
{if(!this._task||!this._testGroups||!this._analysisResults)
return false;let metric=this._metric;this.part('group-pane').setAnalysisResults(this._analysisResults,metric);if(metric){const view=this._analysisResults.viewForMetric(metric);this.part('results-pane').setAnalysisResultsView(view);}
return true;}
render()
{super.render();Instrumentation.startMeasuringTime('AnalysisTaskPage','render');this.content().querySelector('.error-message').textContent=this._errorMessage||'';this._renderTaskNameAndStatusLazily.evaluate(this._task,this._task?this._task.name():null,this._task?this._task.changeType():null);this._renderCauseAndFixesLazily.evaluate(this._startPoint,this._task,this.part('cause-list'),this._task?this._task.causes():[]);this._renderCauseAndFixesLazily.evaluate(this._startPoint,this._task,this.part('fix-list'),this._task?this._task.fixes():[]);this._renderRelatedTasksLazily.evaluate(this._task,this._relatedTasks);this.content('chart-pane').style.display=this._task&&!this._task.isCustom()?null:'none';this.part('chart-pane').setShowForm(!!this._triggerable);this.content('results-pane').style.display=this._task&&!this._task.isCustom()?null:'none';this.part('results-pane').setShowForm(!!this._triggerable);this.content('configurator-pane').style.display=this._task&&this._task.isCustom()?null:'none';Instrumentation.endMeasuringTime('AnalysisTaskPage','render');}
_renderTaskNameAndStatus(task,taskName,changeType)
{this.part('analysis-task-name').setText(taskName);if(task&&!task.isCustom()){const link=ComponentBase.createLink;const platform=task.platform();const metric=task.metric();const subtitle=`${metric.fullName()} on ${platform.label()}`;this.renderReplace(this.content('platform-metric-names'),link(subtitle,this.router().url('charts',ChartsPage.createStateForAnalysisTask(task))));}
this.content('change-type').value=changeType||'unconfirmed';}
_renderRelatedTasks(task,relatedTasks)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;this.renderReplace(this.content('related-tasks-list'),(task&&relatedTasks?relatedTasks:[]).map((otherTask)=>{let suffix='';const taskLabel=otherTask.label();if(otherTask.metric()&&otherTask.metric()!=task.metric()&&taskLabel.indexOf(otherTask.metric().label())<0)
suffix+=` with "${otherTask.metric().label()}"`;if(otherTask.platform()&&otherTask.platform()!=task.platform()&&taskLabel.indexOf(otherTask.platform().label())<0)
suffix+=` on ${otherTask.platform().label()}`;return element('li',[link(taskLabel,this.router().url(`analysis/task/${otherTask.id()}`)),suffix]);}));}
_renderCauseAndFixes(startPoint,task,list,commits)
{const hasData=startPoint&&task;this.content('cause-fix').style.display=hasData?null:'none';if(!hasData)
return;const commitSet=startPoint.commitSet();const repositoryList=Repository.sortByNamePreferringOnesWithURL(commitSet.repositories());const makeItem=(commit)=>{return new MutableListItem(commit.repository(),commit.label(),commit.title(),commit.url(),'Disassociate this commit',this._dissociateCommit.bind(this,commit));}
list.setKindList(repositoryList);list.setList(commits.map((commit)=>makeItem(commit)));}
_showTestGroup(testGroup)
{this._currentTestGroup=testGroup;this.part('configurator-pane').setTestGroups(this._filteredTestGroups,this._currentTestGroup);this.part('results-pane').setTestGroups(this._filteredTestGroups,this._currentTestGroup);const groupsInReverseChronology=this._filteredTestGroups.slice(0).reverse();const showHiddenGroups=!this._testGroups.some((group)=>group.isHidden())||this._showHiddenTestGroups;this.part('group-pane').setTestGroups(groupsInReverseChronology,this._currentTestGroup,showHiddenGroups);this.enqueueToRender();}
_updateTaskName(newName)
{console.assert(this._task);return this._task.updateName(newName).then(()=>{this.enqueueToRender();},(error)=>{this.enqueueToRender();alert('Failed to update the name: '+error);});}
_updateTestGroupName(testGroup,newName)
{return testGroup.updateName(newName).then(()=>{this._showTestGroup(this._currentTestGroup);this.enqueueToRender();},(error)=>{this.enqueueToRender();alert('Failed to hide the test name: '+error);});}
_hideCurrentTestGroup(testGroup)
{return testGroup.updateHiddenFlag(!testGroup.isHidden()).then(()=>{this._didUpdateTestGroupHiddenState();this.enqueueToRender();},function(error){this._mayHaveMutatedTestGroupHiddenState();this.enqueueToRender();alert('Failed to update the group: '+error);});}
_updateChangeType(event)
{event.preventDefault();console.assert(this._task);let newChangeType=this.content('change-type').value;if(newChangeType=='unconfirmed')
newChangeType=null;const updateRendering=()=>{this.part('chart-pane').didUpdateAnnotations();this.enqueueToRender();};return this._task.updateChangeType(newChangeType).then(updateRendering,(error)=>{updateRendering();alert('Failed to update the status: '+error);});}
_associateCommit(kind,repository,revision)
{const updateRendering=()=>{this.enqueueToRender();};return this._task.associateCommit(kind,repository,revision).then(updateRendering,(error)=>{updateRendering();if(error=='AmbiguousRevision')
alert('There are multiple revisions that match the specified string: '+revision);else if(error=='CommitNotFound')
alert('There are no revisions that match the specified string:'+revision);else
alert('Failed to associate the commit: '+error);});}
_dissociateCommit(commit)
{const updateRendering=()=>{this.enqueueToRender();};return this._task.dissociateCommit(commit).then(updateRendering,(error)=>{updateRendering();alert('Failed to dissociate the commit: '+error);});}
async _retryCurrentTestGroup(testGroup,repetitionCount,repetitionType,notifyOnCompletion)
{const existingNames=(this._testGroups||[]).map((group)=>group.name());const newName=CommitSet.createNameWithoutCollision(testGroup.name(),new Set(existingNames));const commitSetList=testGroup.requestedCommitSets();const platform=this._task.platform()||testGroup.platform();try{const testGroups=await TestGroup.createWithCustomConfiguration(this._task,platform,testGroup.test(),newName,repetitionCount,repetitionType,commitSetList,notifyOnCompletion);this._didFetchTestGroups(testGroups);}catch(error){alert('Failed to create a new test group: '+error);}}
async _bisectCurrentTestGroup(testGroup,commitSets,repetitionCount,repetitionType,notifyOnCompletion)
{console.assert(testGroup.task());const existingTestGroupNames=new Set((this._testGroups||[]).map((testGroup)=>testGroup.name()));for(let i=1;i<commitSets.length;i++){const previousCommitSet=commitSets[i-1];const currentCommitSet=commitSets[i];const testGroupName=CommitSet.createNameWithoutCollision(CommitSet.diff(previousCommitSet,currentCommitSet),existingTestGroupNames);try{const testGroups=await TestGroup.createAndRefetchTestGroups(testGroup.task(),testGroupName,repetitionCount,repetitionType,[previousCommitSet,currentCommitSet],notifyOnCompletion);this._didFetchTestGroups(testGroups);}catch(error){alert('Failed to create a new test group: '+error);break;}}}
async _createTestGroupAfterVerifyingCommitSetList(testGroupName,repetitionCount,repetitionType,commitSetMap,notifyOnCompletion)
{if(this._hasDuplicateTestGroupName(testGroupName)){alert(`There is already a test group named "${testGroupName}"`);return;}
const firstLabel=Object.keys(commitSetMap)[0];const firstCommitSet=commitSetMap[firstLabel];for(let currentLabel in commitSetMap){const commitSet=commitSetMap[currentLabel];for(let repository of commitSet.repositories()){if(!firstCommitSet.revisionForRepository(repository))
return alert(`Set ${currentLabel} specifies ${repository.label()} but set ${firstLabel} does not.`);}
for(let repository of firstCommitSet.repositories()){if(!commitSet.revisionForRepository(repository))
return alert(`Set ${firstLabel} specifies ${repository.label()} but set ${currentLabel} does not.`);}}
const commitSets=[];for(let label in commitSetMap)
commitSets.push(commitSetMap[label]);try{const testGroups=await TestGroup.createAndRefetchTestGroups(this._task,testGroupName,repetitionCount,repetitionType,commitSets,notifyOnCompletion);return this._didFetchTestGroups(testGroups);}catch(error){alert('Failed to create a new test group: '+error);}}
async _createCustomTestGroup(testGroupName,repetitionCount,repetitionType,commitSets,platform,test,notifyOnCompletion)
{console.assert(this._task.isCustom());if(this._hasDuplicateTestGroupName(testGroupName)){alert(`There is already a test group named "${testGroupName}"`);return;}
try{const testGroups=await TestGroup.createWithCustomConfiguration(this._task,platform,test,testGroupName,repetitionCount,repetitionType,commitSets,notifyOnCompletion);this._didFetchTestGroups(testGroups);}catch(error){alert('Failed to create a new test group: '+error);}}
_hasDuplicateTestGroupName(name)
{console.assert(this._testGroups);for(var group of this._testGroups){if(group.name()==name)
return true;}
return false;}
static htmlTemplate()
{return` <div class="analysis-task-page"> <h2 class="analysis-task-name"><editable-text id="analysis-task-name"></editable-text></h2> <h3 id="platform-metric-names"></h3> <p class="error-message"></p> <div class="analysis-task-status"> <section> <h3>Status</h3> <form id="change-type-form"> <select id="change-type"> <option value="unconfirmed">Unconfirmed</option> <option value="regression">Definite regression</option> <option value="progression">Definite progression</option> <option value="inconclusive">Inconclusive (Closed)</option> <option value="unchanged">No change (Closed)</option> </select> <button type="submit">Save</button> </form> </section> <section class="associated-bugs"> <h3>Associated Bugs</h3> <analysis-task-bug-list id="bug-list"></analysis-task-bug-list> </section> <section id="cause-fix"> <h3>Caused by</h3> <mutable-list-view id="cause-list"></mutable-list-view> <h3>Fixed by</h3> <mutable-list-view id="fix-list"></mutable-list-view> </section> <section class="related-tasks"> <h3>Related Tasks</h3> <ul id="related-tasks-list"></ul> </section> </div> <analysis-task-chart-pane id="chart-pane"></analysis-task-chart-pane> <analysis-task-results-pane id="results-pane"></analysis-task-results-pane> <analysis-task-configurator-pane id="configurator-pane"></analysis-task-configurator-pane> <analysis-task-test-group-pane id="group-pane"></analysis-task-test-group-pane> </div> `;}
static cssTemplate()
{return`
            .analysis-task-page {
            }

            .analysis-task-name {
                font-size: 1.2rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            #platform-metric-names {
                font-size: 1rem;
                font-weight: inherit;
                color: #c93;
                margin: 0 1rem;
                padding: 0;
            }

            #platform-metric-names a {
                text-decoration: none;
                color: inherit;
            }

            #platform-metric-names:empty {
                visibility: hidden;
                height: 0;
                width: 0;
                /* FIXME: Use display: none instead once r214290 is shipped everywhere */
            }

            .error-message:empty {
                visibility: hidden;
                height: 0;
                width: 0;
                /* FIXME: Use display: none instead once r214290 is shipped everywhere */
            }

            .error-message:not(:empty) {
                margin: 1rem;
                padding: 0;
            }

            #chart-pane,
            #results-pane {
                display: block;
                padding: 0 1rem;
                border-bottom: solid 1px #ccc;
            }

            #results-pane {
                margin-top: 1rem;
            }

            #group-pane {
                margin: 1rem;
                margin-bottom: 2rem;
            }

            .analysis-task-status {
                margin: 0;
                display: flex;
                padding-bottom: 1rem;
                margin-bottom: 1rem;
                border-bottom: solid 1px #ccc;
            }

            .analysis-task-status > section {
                flex-grow: 1;
                flex-shrink: 0;
                border-left: solid 1px #eee;
                padding-left: 1rem;
                padding-right: 1rem;
            }

            .analysis-task-status > section.related-tasks {
                flex-shrink: 1;
            }

            .analysis-task-status > section:first-child {
                border-left: none;
            }

            .analysis-task-status h3 {
                font-size: 1rem;
                font-weight: inherit;
                color: #c93;
            }

            .analysis-task-status ul,
            .analysis-task-status li {
                list-style: none;
                padding: 0;
                margin: 0;
            }

            .related-tasks-list {
                max-height: 10rem;
                overflow-y: scroll;
            }

            .test-configuration h3 {
                font-size: 1rem;
                font-weight: inherit;
                color: inherit;
                margin: 0 1rem;
                padding: 0;
            }`;}}
class CreateAnalysisTaskPage extends PageWithHeading{constructor()
{super('Create Analysis Task');this._message=null;this._renderMessageLazily=new LazilyEvaluatedFunction(this._renderMessage.bind(this));}
title(){return'Creating a New Analysis Task';}
routeName(){return'analysis/task/create';}
updateFromSerializedState(state,isOpen)
{if(state.error instanceof Set)
state.error=Array.from(state.error.values())[0];this._message=state.error||(state.inProgress?'Creating the analysis task page...':'');}
didConstructShadowTree()
{this.part('form').listenToAction('startTesting',this._createAnalysisTaskWithGroup.bind(this));}
async _createAnalysisTaskWithGroup(testGroupName,repetitionCount,repetitionType,commitSets,platform,test,notifyOnCompletion,taskName)
{try{const task=await TestGroup.createWithTask(taskName,platform,test,testGroupName,repetitionCount,repetitionType,commitSets,notifyOnCompletion);location.href=this.router().url(`analysis/task/${task.id()}`);}catch(error){alert('Failed to create a new test group: '+error);}}
render()
{super.render();this._renderMessageLazily.evaluate(this._message);}
_renderMessage(message)
{const messageContainer=this.content('message');messageContainer.textContent=this._message;messageContainer.style.display=this._message?null:'none';this.content('form').style.display=this._message?'none':null;}
static htmlTemplate()
{return` <p id="message"></p> <custom-configuration-test-group-form id="form"></custom-configuration-test-group-form>`;}
static cssTemplate()
{return` #message{text-align:center}#form{margin:1rem}`;}}
class BuildRequestQueuePage extends PageWithHeading{constructor()
{super('build-request-queue-page');this._buildRequestsByTriggerable=new Map;}
routeName(){return'analysis/queue';}
pageTitle(){return'Build Request Queue';}
open(state)
{for(let triggerable of Triggerable.all()){BuildRequest.fetchForTriggerable(triggerable.name()).then((requests)=>{this._buildRequestsByTriggerable.set(triggerable,requests);this.enqueueToRender();});}
AnalysisTask.fetchAll().then(()=>this.enqueueToRender());super.open(state);}
render()
{super.render();const referenceTime=Date.now();this.renderReplace(this.content().querySelector('.triggerable-list'),Triggerable.sortByName(Triggerable.all()).map((triggerable)=>{const buildRequests=this._buildRequestsByTriggerable.get(triggerable)||[];return this._constructBuildRequestTable(referenceTime,triggerable,buildRequests);}));}
_constructBuildRequestTable(referenceTime,triggerable,buildRequests)
{if(!buildRequests.length)
return[];const rowList=[];const requestCountForGroup={};const requestsByGroup={};let previousRow=null;let requestCount=0;for(const request of buildRequests){const groupId=request.testGroupId();if(groupId in requestCountForGroup){requestCountForGroup[groupId]++;requestsByGroup[groupId].push(request);}else{requestCountForGroup[groupId]=1;requestsByGroup[groupId]=[request];}
if(request.hasFinished())
continue;requestCount++;if(previousRow&&previousRow.request.testGroupId()==groupId){if(previousRow.contraction)
previousRow.count++;else
rowList.push({contraction:true,request:previousRow.request,count:1});}else
rowList.push({contraction:false,request:request,count:null});previousRow=rowList[rowList.length-1];}
const element=ComponentBase.createElement;const link=ComponentBase.createLink;const router=this.router();return element('table',{class:'build-request-table'},[element('caption',`${triggerable.name()}: ${requestCount} pending requests`),element('thead',[element('td','Request ID'),element('td','Platform'),element('td','Test'),element('td','Analysis Task'),element('td','Group'),element('td','Order'),element('td','Status'),element('td','Waiting Time'),]),element('tbody',rowList.map((entry)=>{if(entry.contraction){return element('tr',{class:'contraction'},[element('td',{colspan:8},`${entry.count} additional requests`)]);}
const request=entry.request;const taskId=request.analysisTaskId();const task=AnalysisTask.findById(taskId);const requestsForGroup=requestsByGroup[request.testGroupId()];const firstOrder=requestsForGroup[0].order();let testName=null;if(request.test())
testName=request.test().fullName();else{const firstRequestToTest=requestsForGroup.find((request)=>!!request.test());testName=`Building (for ${firstRequestToTest.test().fullName()})`;}
return element('tr',[element('td',{class:'request-id'},request.id()),element('td',{class:'platform'},request.platform().name()),element('td',{class:'test'},testName),element('td',{class:'task'},!task?taskId:link(task.name(),router.url(`analysis/task/${task.id()}`))),element('td',{class:'test-group'},request.testGroupId()),element('td',{class:'order'},`${request.order() - firstOrder + 1} of ${requestCountForGroup[request.testGroupId()]}`),element('td',{class:'status'},request.statusLabel()),element('td',{class:'wait'},request.waitingTime(referenceTime))]);}))]);}
static htmlTemplate()
{return`<div class="triggerable-list"></div>`;}
static cssTemplate()
{return` .triggerable-list{margin:1rem}.build-request-table{border-collapse:collapse;border:solid 0px #ccc;font-size:0.9rem;margin-bottom:2rem}.build-request-table caption{text-align:left;font-size:1.2rem;margin:1rem 0 0.5rem 0;color:#c93}.build-request-table td{padding:0.2rem}.build-request-table td:not(.test):not(.task){white-space:nowrap}.build-request-table .contraction{text-align:center;color:#999}.build-request-table tr:not(.contraction) td{border-top:solid 1px #eee}.build-request-table tr:last-child td{border-bottom:solid 1px #eee}.build-request-table thead{font-weight:inherit;color:#c93}`;}}
class SummaryPage extends PageWithHeading{constructor(summarySettings)
{super(summarySettings.name,null);this._route=summarySettings.route;this._table={heading:summarySettings.platformGroups,groups:[],};this._shouldConstructTable=true;this._renderQueue=[];this._configGroups=[];this._excludedConfigurations=summarySettings.excludedConfigurations;this._weightedConfigurations=summarySettings.weightedConfigurations;for(var metricGroup of summarySettings.metricGroups){var group={name:metricGroup.name,rows:[]};this._table.groups.push(group);for(var subMetricGroup of metricGroup.subgroups){var row={name:subMetricGroup.name,cells:[]};group.rows.push(row);for(var platformGroup of summarySettings.platformGroups)
row.cells.push(this._createConfigurationGroup(platformGroup.platforms,subMetricGroup.metrics));}}}
routeName(){return`summary/${this._route}`;}
open(state)
{super.open(state);var current=Date.now();var timeRange=[current-24*3600*1000,current];for(var group of this._configGroups)
group.fetchAndComputeSummary(timeRange).then(()=>{this.enqueueToRender();});}
render()
{Instrumentation.startMeasuringTime('SummaryPage','render');super.render();if(this._shouldConstructTable){Instrumentation.startMeasuringTime('SummaryPage','_constructTable');this.renderReplace(this.content().querySelector('.summary-table'),this._constructTable());Instrumentation.endMeasuringTime('SummaryPage','_constructTable');}
for(var render of this._renderQueue)
render();Instrumentation.endMeasuringTime('SummaryPage','render');}
_createConfigurationGroup(platformIdList,metricIdList)
{var platforms=platformIdList.map(function(id){return Platform.findById(id);}).filter(function(obj){return!!obj;});var metrics=metricIdList.map(function(id){return Metric.findById(id);}).filter(function(obj){return!!obj;});var configGroup=new SummaryPageConfigurationGroup(platforms,metrics,this._excludedConfigurations,this._weightedConfigurations);this._configGroups.push(configGroup);return configGroup;}
_constructTable()
{var element=ComponentBase.createElement;var self=this;this._shouldConstructTable=false;this._renderQueue=[];return[element('thead',element('tr',[element('td',{colspan:2}),this._table.heading.map(function(group){var nodes=[group.name];if(group.subtitle){nodes.push(element('br'));nodes.push(element('span',{class:'subtitle'},group.subtitle));}
return element('td',nodes);}),])),this._table.groups.map(function(rowGroup){return element('tbody',rowGroup.rows.map(function(row,rowIndex){var headings;headings=[element('th',{class:'minorHeader'},row.name)];if(!rowIndex)
headings.unshift(element('th',{class:'majorHeader',rowspan:rowGroup.rows.length},rowGroup.name));return element('tr',[headings,row.cells.map(self._constructRatioGraph.bind(self))]);}));}),];}
_constructRatioGraph(configurationGroup)
{var element=ComponentBase.createElement;var link=ComponentBase.createLink;var configurationList=configurationGroup.configurationList();var ratioGraph=new RatioBarGraph();if(configurationList.length==0){this._renderQueue.push(()=>{ratioGraph.enqueueToRender();});return element('td',ratioGraph);}
var state=ChartsPage.createStateForConfigurationList(configurationList);var anchor=link(ratioGraph,this.router().url('charts',state));var spinner=new SpinnerIcon;var cell=element('td',[anchor,spinner]);this._renderQueue.push(this._renderCell.bind(this,cell,spinner,anchor,ratioGraph,configurationGroup));return cell;}
_renderCell(cell,spinner,anchor,ratioGraph,configurationGroup)
{if(configurationGroup.isFetching())
cell.classList.add('fetching');else
cell.classList.remove('fetching');var warningText=this._warningTextForGroup(configurationGroup);anchor.title=warningText||'Open charts';ratioGraph.update(configurationGroup.ratio(),configurationGroup.label(),!!warningText);ratioGraph.enqueueToRender();}
_warningTextForGroup(configurationGroup)
{function mapAndSortByName(platforms)
{return platforms&&platforms.map(function(platform){return platform.label();}).sort();}
function pluralizeIfNeeded(singularWord,platforms){return singularWord+(platforms.length>1?'s':'');}
var warnings=[];var missingPlatforms=mapAndSortByName(configurationGroup.missingPlatforms());if(missingPlatforms)
warnings.push(`Missing ${pluralizeIfNeeded('platform', missingPlatforms)}: ${missingPlatforms.join(', ')}`);var platformsWithoutBaselines=mapAndSortByName(configurationGroup.platformsWithoutBaseline());if(platformsWithoutBaselines)
warnings.push(`Need ${pluralizeIfNeeded('baseline', platformsWithoutBaselines)}: ${platformsWithoutBaselines.join(', ')}`);return warnings.length?warnings.join('\n'):null;}
static htmlTemplate()
{return`<section class="page-with-heading"><table class="summary-table"></table></section>`;}
static cssTemplate()
{return` .summary-table{border-collapse:collapse;border:none;margin:0;width:100%}.summary-table td,.summary-table th{text-align:center;padding:0px}.summary-table .majorHeader{width:5rem}.summary-table .minorHeader{width:7rem}.summary-table .unifiedHeader{padding-left:5rem}.summary-table tbody tr:first-child>*{border-top:solid 1px #ddd}.summary-table tbody tr:nth-child(even)>*:not(.majorHeader){background:#f9f9f9}.summary-table th,.summary-table thead td{color:#333;font-weight:inherit;font-size:1rem;padding:0.2rem 0.4rem}.summary-table thead td{font-size:1.2rem;line-height:1.3rem}.summary-table .subtitle{display:block;font-size:0.9rem;line-height:1.2rem;color:#666}.summary-table tbody td{position:relative;font-weight:inherit;font-size:0.9rem;height:2.5rem;padding:0}.summary-table td>*{height:100%}.summary-table td spinner-icon{display:block;position:absolute;top:0.25rem;left:calc(50% - 1rem);z-index:100}.summary-table td.fetching a{display:none}.summary-table td:not(.fetching) spinner-icon{display:none}`;}}
class SummaryPageConfigurationGroup{constructor(platforms,metrics,excludedConfigurations,weightedConfigurations)
{this._measurementSets=[];this._configurationList=[];this._setToRatio=new Map;this._ratio=NaN;this._label=null;this._missingPlatforms=new Set;this._platformsWithoutBaseline=new Set;this._isFetching=false;this._smallerIsBetter=metrics.length?metrics[0].isSmallerBetter():null;this._weightedConfigurations=weightedConfigurations;for(const platform of platforms){console.assert(platform instanceof Platform);let foundInSomeMetric=false;let excludedMerticCount=0;for(const metric of metrics){console.assert(metric instanceof Metric);console.assert(this._smallerIsBetter==metric.isSmallerBetter());metric.isSmallerBetter();if(excludedConfigurations&&platform.id()in excludedConfigurations&&excludedConfigurations[platform.id()].includes(+metric.id())){excludedMerticCount+=1;continue;}
if(!platform.hasMetric(metric))
continue;foundInSomeMetric=true;this._measurementSets.push(MeasurementSet.findSet(platform.id(),metric.id(),platform.lastModified(metric)));this._configurationList.push([platform.id(),metric.id()]);}
if(!foundInSomeMetric&&excludedMerticCount<metrics.length)
this._missingPlatforms.add(platform);}}
ratio(){return this._ratio;}
label(){return this._label;}
changeType(){return this._changeType;}
configurationList(){return this._configurationList;}
isFetching(){return this._isFetching;}
missingPlatforms(){return this._missingPlatforms.size?Array.from(this._missingPlatforms):null;}
platformsWithoutBaseline(){return this._platformsWithoutBaseline.size?Array.from(this._platformsWithoutBaseline):null;}
fetchAndComputeSummary(timeRange)
{console.assert(timeRange instanceof Array);console.assert(typeof(timeRange[0])=='number');console.assert(typeof(timeRange[1])=='number');var promises=[];for(var set of this._measurementSets)
promises.push(this._fetchAndComputeRatio(set,timeRange));var self=this;var fetched=false;setTimeout(function(){if(!fetched)
self._isFetching=true;},50);return Promise.all(promises).then(function(){fetched=true;self._isFetching=false;self._computeSummary();});}
_computeSummary()
{var ratios=[];for(var set of this._measurementSets){const ratio=this._setToRatio.get(set);const weight=this._weightForMeasurementSet(set);if(!isNaN(ratio))
ratios.push({value:ratio,weight});}
var averageRatio=Statistics.weightedMean(ratios);if(isNaN(averageRatio))
return;var currentIsSmallerThanBaseline=averageRatio<1;var changeType=this._smallerIsBetter==currentIsSmallerThanBaseline?'better':'worse';averageRatio=Math.abs(averageRatio-1);this._ratio=averageRatio*(changeType=='better'?1:-1);this._label=(averageRatio*100).toFixed(1)+'%';this._changeType=changeType;}
_weightForMeasurementSet(measurementSet)
{if(!this._weightedConfigurations)
return 1;const platformId=measurementSet.platformId();const weightForPlatform=this._weightedConfigurations[platformId];if(!weightForPlatform)
return 1;if(!isNaN(+weightForPlatform))
return+weightForPlatform;const metricId=measurementSet.metricId();if(typeof weightForPlatform!='object'||isNaN(+weightForPlatform[metricId]))
return 1
return+weightForPlatform[metricId];}
_fetchAndComputeRatio(set,timeRange)
{var setToRatio=this._setToRatio;var self=this;return set.fetchBetween(timeRange[0],timeRange[1]).then(function(){var baselineTimeSeries=set.fetchedTimeSeries('baseline',false,false);var currentTimeSeries=set.fetchedTimeSeries('current',false,false);const baselineMean=SummaryPageConfigurationGroup._meanForTimeRange(baselineTimeSeries,timeRange);const currentMean=SummaryPageConfigurationGroup._meanForTimeRange(currentTimeSeries,timeRange);var platform=Platform.findById(set.platformId());if(!currentMean)
self._missingPlatforms.add(platform);else if(!baselineMean)
self._platformsWithoutBaseline.add(platform);setToRatio.set(set,currentMean/baselineMean);}).catch(function(){setToRatio.set(set,NaN);});}
static _startAndEndPointForTimeRange(timeSeries,timeRange)
{if(!timeSeries.firstPoint())
return NaN;const startPoint=timeSeries.findPointAfterTime(timeRange[0])||timeSeries.lastPoint();const afterEndPoint=timeSeries.findPointAfterTime(timeRange[1])||timeSeries.lastPoint();let endPoint=timeSeries.previousPoint(afterEndPoint);if(!endPoint||startPoint==afterEndPoint)
endPoint=afterEndPoint;return[startPoint,endPoint];}
static _meanForTimeRange(timeSeries,timeRange)
{const[startPoint,endPoint]=SummaryPageConfigurationGroup._startAndEndPointForTimeRange(timeSeries,timeRange);return Statistics.mean(timeSeries.viewBetweenPoints(startPoint,endPoint).values());}}
class TestFreshnessPage extends PageWithHeading{constructor(summaryPageConfiguration,testAgeToleranceInHours)
{super('test-freshness',null);this._testAgeTolerance=(testAgeToleranceInHours||24)*3600*1000;this._timeDuration=this._testAgeTolerance*2;this._excludedConfigurations={};this._lastDataPointByConfiguration=null;this._indicatorByConfiguration=null;this._renderTableLazily=new LazilyEvaluatedFunction(this._renderTable.bind(this));this._hoveringIndicator=null;this._indicatorForTooltip=null;this._firstIndicatorAnchor=null;this._showTooltip=false;this._builderByIndicator=null;this._tabIndexForIndicator=null;this._coordinateForIndicator=null;this._indicatorAnchorGrid=null;this._skipNextClick=false;this._skipNextStateCleanOnScroll=false;this._lastFocusedCell=null;this._renderTooltipLazily=new LazilyEvaluatedFunction(this._renderTooltip.bind(this));this._loadConfig(summaryPageConfiguration);}
name(){return'Test-Freshness';}
_loadConfig(summaryPageConfiguration)
{const platformIdSet=new Set;const metricIdSet=new Set;for(const config of summaryPageConfiguration){for(const platformGroup of config.platformGroups){for(const platformId of platformGroup.platforms)
platformIdSet.add(platformId);}
for(const metricGroup of config.metricGroups){for(const subgroup of metricGroup.subgroups){for(const metricId of subgroup.metrics)
metricIdSet.add(metricId);}}
const excludedConfigs=config.excludedConfigurations;for(const platform in excludedConfigs){if(platform in this._excludedConfigurations)
this._excludedConfigurations[platform]=this._excludedConfigurations[platform].concat(excludedConfigs[platform]);else
this._excludedConfigurations[platform]=excludedConfigs[platform];}}
this._platforms=[...platformIdSet].map((platformId)=>Platform.findById(platformId));this._metrics=[...metricIdSet].map((metricId)=>Metric.findById(metricId));}
open(state)
{this._fetchTestResults();super.open(state);}
didConstructShadowTree()
{super.didConstructShadowTree();const tooltipTable=this.content('tooltip-table');this.content().addEventListener('click',(event)=>{if(!tooltipTable.contains(event.target))
this._clearIndicatorState(false);});tooltipTable.onkeydown=this.createEventHandler((event)=>{if(event.code=='Escape'){event.preventDefault();event.stopPropagation();this._lastFocusedCell.focus({preventScroll:true});}},{preventDefault:false,stopPropagation:false});window.addEventListener('keydown',(event)=>{if(!['ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(event.code))
return;event.preventDefault();if(!this._indicatorForTooltip&&!this._hoveringIndicator){if(this._firstIndicatorAnchor)
this._firstIndicatorAnchor.focus({preventScroll:true});return;}
let[row,column]=this._coordinateForIndicator.get(this._indicatorForTooltip||this._hoveringIndicator);let direction=null;switch(event.code){case'ArrowUp':row-=1;break;case'ArrowDown':row+=1;break;case'ArrowLeft':column-=1;direction='leftOnly'
break;case'ArrowRight':column+=1;direction='rightOnly'}
const closestIndicatorAnchor=this._findClosestIndicatorAnchorForCoordinate(column,row,this._indicatorAnchorGrid,direction);if(closestIndicatorAnchor)
closestIndicatorAnchor.focus({preventScroll:true});});}
_findClosestIndicatorAnchorForCoordinate(columnIndex,rowIndex,grid,direction)
{rowIndex=Math.min(Math.max(rowIndex,0),grid.length-1);const row=grid[rowIndex];if(!row.length)
return null;const start=Math.min(Math.max(columnIndex,0),row.length-1);if(row[start])
return row[start];let offset=1;while(true){const leftIndex=start-offset;if(leftIndex>=0&&row[leftIndex]&&direction!='rightOnly')
return row[leftIndex];const rightIndex=start+offset;if(rightIndex<row.length&&row[rightIndex]&&direction!='leftOnly')
return row[rightIndex];if(leftIndex<0&&rightIndex>=row.length)
break;offset+=1;}
return null;}
_fetchTestResults()
{this._measurementSetFetchTime=Date.now();this._lastDataPointByConfiguration=new Map;this._builderByIndicator=new Map;const startTime=this._measurementSetFetchTime-this._timeDuration;for(const platform of this._platforms){const lastDataPointByMetric=new Map;this._lastDataPointByConfiguration.set(platform,lastDataPointByMetric);for(const metric of this._metrics){if(!this._isValidPlatformMetricCombination(platform,metric,this._excludedConfigurations))
continue;const measurementSet=MeasurementSet.findSet(platform.id(),metric.id(),platform.lastModified(metric));measurementSet.fetchBetween(startTime,this._measurementSetFetchTime).then(()=>{const currentTimeSeries=measurementSet.fetchedTimeSeries('current',false,false);let timeForLatestBuild=startTime;let lastBuild=null;let builder=null;let commitSetOfLastPoint=null;const lastPoint=currentTimeSeries.lastPoint();if(lastPoint){timeForLatestBuild=lastPoint.build().buildTime().getTime();const view=currentTimeSeries.viewBetweenPoints(currentTimeSeries.firstPoint(),lastPoint);for(const point of view){const build=point.build();if(!build)
continue;if(build.buildTime().getTime()>=timeForLatestBuild){timeForLatestBuild=build.buildTime().getTime();lastBuild=build;builder=build.builder();}}
commitSetOfLastPoint=lastPoint.commitSet();}
lastDataPointByMetric.set(metric,{time:timeForLatestBuild,hasCurrentDataPoint:!!lastPoint,lastBuild,builder,commitSetOfLastPoint});this.enqueueToRender();});}}}
render()
{super.render();this._renderTableLazily.evaluate(this._platforms,this._metrics);let buildSummaryForFocusingIndicator=null;let buildForFocusingIndicator=null;let commitSetForFocusingdIndicator=null;let chartURLForFocusingIndicator=null;let platformForFocusingIndicator=null;let metricForFocusingIndicator=null;const builderForFocusingIndicator=this._indicatorForTooltip?this._builderByIndicator.get(this._indicatorForTooltip):null;const builderForHoveringIndicator=this._hoveringIndicator?this._builderByIndicator.get(this._hoveringIndicator):null;for(const[platform,lastDataPointByMetric]of this._lastDataPointByConfiguration.entries()){for(const[metric,lastDataPoint]of lastDataPointByMetric.entries()){const timeDuration=this._measurementSetFetchTime-lastDataPoint.time;const timeDurationSummaryPrefix=lastDataPoint.hasCurrentDataPoint?'':'More than ';const timeDurationSummary=BuildRequest.formatTimeInterval(timeDuration);const summary=`${timeDurationSummaryPrefix}${timeDurationSummary} since latest data point.`;const indicator=this._indicatorByConfiguration.get(platform).get(metric);if(this._indicatorForTooltip&&this._indicatorForTooltip===indicator){buildSummaryForFocusingIndicator=summary;buildForFocusingIndicator=lastDataPoint.lastBuild;commitSetForFocusingdIndicator=lastDataPoint.commitSetOfLastPoint;chartURLForFocusingIndicator=this._router.url('charts',ChartsPage.createStateForDashboardItem(platform.id(),metric.id(),this._measurementSetFetchTime-this._timeDuration));platformForFocusingIndicator=platform;metricForFocusingIndicator=metric;}
this._builderByIndicator.set(indicator,lastDataPoint.builder);const highlighted=builderForFocusingIndicator&&builderForFocusingIndicator==lastDataPoint.builder||builderForHoveringIndicator&&builderForHoveringIndicator===lastDataPoint.builder;indicator.update(timeDuration,this._testAgeTolerance,highlighted);}}
this._renderTooltipLazily.evaluate(this._indicatorForTooltip,this._showTooltip,buildSummaryForFocusingIndicator,buildForFocusingIndicator,commitSetForFocusingdIndicator,chartURLForFocusingIndicator,platformForFocusingIndicator,metricForFocusingIndicator,this._tabIndexForIndicator.get(this._indicatorForTooltip));}
_renderTooltip(indicator,showTooltip,buildSummary,build,commitSet,chartURL,platform,metric,tabIndex)
{if(!indicator||!buildSummary||!showTooltip){this.content('tooltip-anchor').style.display=showTooltip?null:'none';return;}
const element=ComponentBase.createElement;const link=ComponentBase.createLink;const rect=indicator.element().getBoundingClientRect();const tooltipAnchor=this.content('tooltip-anchor');tooltipAnchor.style.display=null;tooltipAnchor.style.top=rect.top+'px';tooltipAnchor.style.left=rect.left+rect.width/2+'px';let tableContent=[element('tr',element('td',{colspan:2},buildSummary))];if(chartURL){const linkDescription=`${metric.test().name()} on ${platform.label()}`;tableContent.push(element('tr',[element('td','Chart'),element('td',{colspan:2},link(linkDescription,linkDescription,chartURL,true,tabIndex))]));}
if(commitSet){if(commitSet.repositories().length)
tableContent.push(element('tr',element('th',{colspan:2},'Latest build information')));tableContent.push(Repository.sortByNamePreferringOnesWithURL(commitSet.repositories()).map((repository)=>{const commit=commitSet.commitForRepository(repository);return element('tr',[element('td',repository.name()),element('td',commit.url()?link(commit.label(),commit.label(),commit.url(),true,tabIndex):commit.label())]);}));}
if(build){const url=build.url();const buildTag=build.buildTag();tableContent.push(element('tr',[element('td','Build'),element('td',{colspan:2},[url?link(buildTag,build.label(),url,true,tabIndex):buildTag]),]));}
this.renderReplace(this.content("tooltip-table"),tableContent);}
_renderTable(platforms,metrics)
{const element=ComponentBase.createElement;const tableHeadElements=[element('th',{class:'table-corner row-head'},'Platform \\ Test')];for(const metric of metrics)
tableHeadElements.push(element('th',{class:'diagonal-head'},element('div',metric.test().fullName())));this._indicatorByConfiguration=new Map;this._coordinateForIndicator=new Map;this._tabIndexForIndicator=new Map;this._indicatorAnchorGrid=[];this._firstIndicatorAnchor=null;let currentTabIndex=1;const tableBodyElement=platforms.map((platform,rowIndex)=>{const indicatorByMetric=new Map;this._indicatorByConfiguration.set(platform,indicatorByMetric);let indicatorAnchorsInCurrentRow=[];const cells=metrics.map((metric,columnIndex)=>{const[cell,anchor,indicator]=this._constructTableCell(platform,metric,currentTabIndex);indicatorAnchorsInCurrentRow.push(anchor);if(!indicator)
return cell;indicatorByMetric.set(metric,indicator);this._tabIndexForIndicator.set(indicator,currentTabIndex);this._coordinateForIndicator.set(indicator,[rowIndex,columnIndex]);++currentTabIndex;if(!this._firstIndicatorAnchor)
this._firstIndicatorAnchor=anchor;return cell;});this._indicatorAnchorGrid.push(indicatorAnchorsInCurrentRow);const row=element('tr',[element('th',{class:'row-head'},platform.label()),...cells]);return row;});const tableBody=element('tbody',tableBodyElement);tableBody.onscroll=this.createEventHandler(()=>this._clearIndicatorState(true));this.renderReplace(this.content('test-health'),[element('thead',tableHeadElements),tableBody]);}
_isValidPlatformMetricCombination(platform,metric)
{return!(this._excludedConfigurations&&this._excludedConfigurations[platform.id()]&&this._excludedConfigurations[platform.id()].some((metricId)=>metricId==metric.id()))&&platform.hasMetric(metric);}
_constructTableCell(platform,metric,tabIndex)
{const element=ComponentBase.createElement;const link=ComponentBase.createLink;if(!this._isValidPlatformMetricCombination(platform,metric))
return[element('td',{class:'blank-cell'},element('div')),null,null];const indicator=new FreshnessIndicator;const anchor=link(indicator,'',()=>{if(this._skipNextClick){this._skipNextClick=false;return;}
this._showTooltip=!this._showTooltip;this.enqueueToRender();},false,tabIndex);const cell=element('td',{class:'status-cell'},anchor);this._configureAnchorForIndicator(anchor,indicator);return[cell,anchor,indicator];}
_configureAnchorForIndicator(anchor,indicator)
{anchor.onmouseover=this.createEventHandler(()=>{this._hoveringIndicator=indicator;this.enqueueToRender();});anchor.onmousedown=this.createEventHandler(()=>this._skipNextClick=this._indicatorForTooltip!=indicator,{preventDefault:false,stopPropagation:false});anchor.onfocus=this.createEventHandler(()=>{this._showTooltip=this._indicatorForTooltip!=indicator;this._hoveringIndicator=indicator;this._indicatorForTooltip=indicator;this._lastFocusedCell=anchor;this._skipNextStateCleanOnScroll=true;this.enqueueToRender();});anchor.onkeydown=this.createEventHandler((event)=>{if(event.code=='Escape'){event.preventDefault();event.stopPropagation();this._showTooltip=event.code=='Enter'?!this._showTooltip:false;this.enqueueToRender();}},{preventDefault:false,stopPropagation:false});}
_clearIndicatorState(dueToScroll)
{if(this._skipNextStateCleanOnScroll){this._skipNextStateCleanOnScroll=false;if(dueToScroll)
return;}
this._showTooltip=false;this._indicatorForTooltip=null;this._hoveringIndicator=null;this.enqueueToRender();}
static htmlTemplate()
{return`<section class="page-with-heading"> <table id="test-health"></table> <div id="tooltip-anchor"> <table id="tooltip-table"></table> </div> </section>`;}
static cssTemplate()
{return` .page-with-heading{display:grid;grid-template-columns:1fr [content] min-content 1fr}#test-health{font-size:1rem;grid-column:content}#test-health thead{display:block;align:right}#test-health th.table-corner{text-align:right;vertical-align:bottom}#test-health .row-head{min-width:24rem}#test-health th{text-align:left;border-bottom:0.1rem solid #ccc;font-weight:normal}#test-health th.diagonal-head{white-space:nowrap;height:16rem;width:2.2rem;border-bottom:0rem;padding:0}#test-health th.diagonal-head>div{transform:translate(1.1rem,7.5rem) rotate(315deg);transform-origin:center left;width:2.2rem;border:0rem}#test-health tbody{display:block;overflow:auto;height:calc(100vh - 24rem)}#test-health td.status-cell{position:relative;margin:0;padding:0;max-width:2.2rem;max-height:2.2rem;min-width:2.2rem;min-height:2.2rem}#test-health td.status-cell>a{display:block}#test-health td.status-cell>a:focus{outline:none}#test-health td.status-cell>a:focus::after{position:absolute;content:"";bottom:-0.1rem;left:50%;margin-left:-0.2rem;height:0rem;border-width:0.2rem;border-style:solid;border-color:transparent transparent red transparent;outline:none}#test-health td.blank-cell{margin:0;padding:0;max-width:2.2rem;max-height:2.2rem;min-width:2.2rem;min-height:2.2rem}#test-health td.blank-cell>div{background-color:#F9F9F9;height:1.6rem;width:1.6rem;margin:auto;padding:0;position:relative;overflow:hidden}#test-health td.blank-cell>div::before{content:"";position:absolute;top:-1px;left:-1px;display:block;width:0px;height:0px;border-right:calc(1.6rem + 1px) solid #ddd;border-top:calc(1.6rem + 1px) solid transparent}#test-health td.blank-cell>div::after{content:"";display:block;position:absolute;top:1px;left:1px;width:0px;height:0px;border-right:calc(1.6rem - 1px) solid #F9F9F9;border-top:calc(1.6rem - 1px) solid transparent}#tooltip-anchor{width:0;height:0;position:absolute}#tooltip-table{position:absolute;width:22rem;background-color:#696969;opacity:0.96;margin:0.3rem;padding:0.3rem;border-radius:0.4rem;z-index:1;text-align:center;display:inline-table;color:white;bottom:0;left:-11.3rem}#tooltip-table td{overflow:hidden;max-width:22rem;text-overflow:ellipsis}#tooltip-table::after{content:" ";position:absolute;top:100%;left:50%;margin-left:-0.3rem;border-width:0.3rem;border-style:solid;border-color:#696969 transparent transparent transparent}#tooltip-table a{color:white;font-weight:bold}#tooltip-table a:focus{background-color:#AAB7B8;outline:none}`;}
routeName(){return'test-freshness';}}
class SpinningPage extends Page{static htmlTemplate()
{return`<div style="position: absolute; width: 100%; padding-top: 25%; text-align: center;"><spinner-icon></spinner-icon></div>`;}}
function main(){const requiredFeatures={'Shadow DOM API':()=>{return!!Element.prototype.attachShadow;},};for(const name in requiredFeatures){if(!requiredFeatures[name]())
return alert(`Your browser does not support ${name}. Try using the latest Safari or Chrome.`);} 
mockAPIs();(new SpinningPage).open();Manifest.fetch().then(function(manifest){const dashboardToolbar=new DashboardToolbar;const dashboardPages=[];if(manifest.dashboards){for(const name in manifest.dashboards)
dashboardPages.push(new DashboardPage(name,manifest.dashboards[name],dashboardToolbar));}
const summaryPages=[];let testFreshnessPage=null;if(manifest.summaryPages){for(const summaryPage of manifest.summaryPages)
summaryPages.push(new SummaryPage(summaryPage));testFreshnessPage=new TestFreshnessPage(manifest.summaryPages,manifest.testAgeToleranceInHours);}
const chartsToolbar=new ChartsToolbar;const chartsPage=new ChartsPage(chartsToolbar);const analysisCategoryPage=new AnalysisCategoryPage();if(testFreshnessPage)
summaryPages.push(testFreshnessPage);const createAnalysisTaskPage=new CreateAnalysisTaskPage();createAnalysisTaskPage.setParentPage(analysisCategoryPage);const analysisTaskPage=new AnalysisTaskPage();analysisTaskPage.setParentPage(analysisCategoryPage);const buildRequestQueuePage=new BuildRequestQueuePage();buildRequestQueuePage.setParentPage(analysisCategoryPage);const heading=new Heading(manifest.siteTitle);heading.addPageGroup([chartsPage,analysisCategoryPage,...summaryPages]);heading.setTitle(manifest.siteTitle);heading.addPageGroup(dashboardPages);const router=new PageRouter();for(const summaryPage of summaryPages)
router.addPage(summaryPage);router.addPage(chartsPage);router.addPage(analysisCategoryPage);router.addPage(createAnalysisTaskPage);router.addPage(analysisTaskPage);router.addPage(buildRequestQueuePage);for(const page of dashboardPages)
router.addPage(page);if(summaryPages.length)
router.setDefaultPage(summaryPages[0]);else if(dashboardPages.length)
router.setDefaultPage(dashboardPages[0]);else
router.setDefaultPage(chartsPage);heading.setRouter(router);router.route();}).catch(function(error){alert('Failed to load the site manifest: '+error);});}
if(document.readyState!='loading')
main();else
document.addEventListener('DOMContentLoaded',main);
