//
//  ORForest.cpp
//
//  Created on: 17 Mar 2015
//      Author: Guotai Wang
//
// Copyright (c) 2014-2016 University College London, United Kingdom. All rights reserved.
// http://cmictig.cs.ucl.ac.uk
//
// Distributed under the BSD-3 licence. Please see the file licence.txt
//

#include "ORForest.h"
#include "Node.h"
#include <iostream>
#include <ctime>

template<typename T>
RandomForest::ORForest<T>::ORForest()
{
	treeNumber=20;
	maxDepth=10;
	leastNsample=10;
    trainData=nullptr;;
    testData=nullptr;
    testDataRatio=0.3;
    balanceType=SingleParameterBoostrap;
    samplingType=DownSamplingMajority;
    onlineUpdate=true;
}

template<typename T>
RandomForest::ORForest<T>::~ORForest()
{
}

template<typename T>
void RandomForest::ORForest<T>::Init(int Ntree,int treeDepth, int leastNsampleForSplit)
{
	treeNumber=Ntree;
	maxDepth=treeDepth;
	leastNsample=leastNsampleForSplit;
}

template<typename T>
void RandomForest::ORForest<T>::Clear()
{
    if(trees.size()>0)
    {
        trees.resize(0);
    }
    if(trainData)
    {
        trainData.reset();
        trainData=nullptr;;
    }
    if(testData)
    {
        testData.reset();
        testData=nullptr;
    }
}

template<typename T>
void RandomForest::ORForest<T>::SetBalanceType(BalanceType type)
{
    balanceType=type;
}

template<typename T>
RandomForest::BalanceType RandomForest::ORForest<T>::GetBalanceType() const
{
    return balanceType;
}

template<typename T>
void RandomForest::ORForest<T>::SetSamplingType(RandomForest::SamplingType type)
{
    samplingType=type;
}

template<typename T>
RandomForest::SamplingType RandomForest::ORForest<T>::GetSamplingType() const
{
    return samplingType;
}

template<typename T>
void RandomForest::ORForest<T>::DisableOnlineUpdate()
{
    onlineUpdate=false;
}

template<typename T>
void RandomForest::ORForest<T>::GetTrainAndTestData(const std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > i_trainData)
{
    int i_addNs=i_trainData->size();
    int posNadd=0, negNadd=0;
    for(int i=0;i<i_addNs;i++)
    {
        T tempLabel=i_trainData->at(i)->back();
        if(tempLabel==1)
        {
            posNadd++;
        }
        else
        {
            negNadd++;
        }
    }
    if(trainData==nullptr && (posNadd==0 || negNadd==0))
    {
        printf("addNs %d, posNadd %d, negNadd %d, the class number of training data shoule be 2!\n", i_addNs, posNadd, negNadd);
        return;
    }

    int addNTest=i_addNs*testDataRatio;
    int addNTrain=i_addNs-addNTest;
    
    // select train and test data randomly
    std::vector<bool> testDataMask;
    for (int i=0; i<i_addNs; i++)
    {
        testDataMask.push_back(false);
    }

    int add_nTest=0;
    while (add_nTest<addNTest)
    {
        double randf=static_cast<double>(rand())/RAND_MAX;
        int tempIdx=i_addNs*randf;
        if(testDataMask[tempIdx]==false)
        {
            testDataMask[tempIdx]=true;
            add_nTest++;
        }
    }

    std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > addTrainData(new std::vector<std::shared_ptr<std::vector<T> > >);
    std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > addTestData(new std::vector<std::shared_ptr<std::vector<T> > >);
    addTrainData->reserve(addNTrain);
    addTestData->reserve(addNTest);
    for (int i=0; i<i_addNs; i++)
    {
        if(testDataMask[i])
        {
            addTestData->push_back(i_trainData->at(i));
        }
        else{
            addTrainData->push_back(i_trainData->at(i));
        }
    }
    
    std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > newTrainData(new std::vector<std::shared_ptr<std::vector<T> > >);
    std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > newTestData(new std::vector<std::shared_ptr<std::vector<T> > >);
    if(trainData==nullptr)
    {
        newTrainData=addTrainData;
        newTestData=addTestData;
    }
    else{
        newTrainData->insert(newTrainData->end(), trainData->begin(), trainData->end());
        newTrainData->insert(newTrainData->end(),addTrainData->begin(), addTrainData->end());
        
        newTestData->insert(newTestData->end(), testData->begin(), testData->end());
        newTestData->insert(newTestData->end(), addTestData->begin(), addTestData->end());
    }
    trainData=newTrainData;
    testData=newTestData;
 }

template<typename T>
void RandomForest::ORForest<T>::Train(const T *i_trainData, int i_Ns, int i_Nfp1)
{
    std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > tempTrainData(new std::vector<std::shared_ptr<std::vector<T> > >);
    tempTrainData->resize(i_Ns);
    for (int i=0; i<i_Ns; i++)
    {
        std::shared_ptr<std::vector<T> > tempSample(new std::vector<T>);
        tempSample->resize(i_Nfp1);
        for (int j=0; j<i_Nfp1; j++)
        {
            tempSample->at(j)=*(i_trainData+i*i_Nfp1+j);
        }
        tempTrainData->at(i)=tempSample;
    }
    Train(tempTrainData);
}

template<typename T>
void RandomForest::ORForest<T>::Train(const std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > > i_trainData)
{
    if(trees.size()==0 || onlineUpdate==false)
    {
        int posN=0, negN=0;
        while(posN==0 || negN==0)
        {
            GetTrainAndTestData(i_trainData);
            for (int i=0; i<trainData->size(); i++)
            {
                T tempL=trainData->at(i)->back();
                if(tempL==1.0)posN++;
                else negN++;
            }
        }

        trees.resize(treeNumber);
        for(int i=0;i<treeNumber;i++)
		{
            ODTree<T> tempTree;
            trees[i]=tempTree;
			trees[i].SetDepthUpperBound(maxDepth);
            trees[i].SetSampleNumberThreshold(leastNsample);
            trees[i].SetBalanceType(balanceType);
            trees[i].SetSamplingType(samplingType);
			trees[i].Train(trainData);
		}
	}
	else
	{
        // combine old training data and newly arrived training data
        GetTrainAndTestData(i_trainData);
        for (TreeListIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
		{
            tree_iter->Train(trainData);
            T oobe = tree_iter->GetOOBE(testData);
            if (oobe>0.4)
			{
                tree_iter->Reset();
                tree_iter->Train(trainData);
			}
		}
	}
}

template<typename T>
void RandomForest::ORForest<T>::Predict(const std::shared_ptr<std::vector<std::shared_ptr<std::vector<T> > > >& i_testData, std::vector<float>  ** o_predict)
{
    std::vector<float> *sumPredict=new std::vector<float>;
    sumPredict->resize(i_testData->size());

    for(int i=0;i<i_testData->size();i++)
    {
        sumPredict->at(i)=0;
    }

    for (TreeListIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
    {
        std::vector<float> * tempPredict;
        tree_iter->Predict(i_testData, &tempPredict);
        for(int j=0;j<sumPredict->size();j++)
        {
            sumPredict->at(j)+=tempPredict->at(j);
        }
    }
    for(int i=0;i<sumPredict->size();i++)
    {
        sumPredict->at(i)=sumPredict->at(i)/treeNumber;
    }
    *o_predict=sumPredict;
}

template<typename T>
int RandomForest::ORForest<T>::GetActureMaxTreeDepth() const
{
    int tempD=0;
    for (TreeListConstIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
    {
        if (tempD < tree_iter->GetActureTreeDepth())
        {
            tempD = tree_iter->GetActureTreeDepth();
        }
    }
    return tempD;
}

template<typename T>
int RandomForest::ORForest<T>::GetActureMaxTreeNode() const
{
    int n=0;
    for (TreeListConstIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
    {
        if (n < tree_iter->GetActureTreeNode())
        {
            n = tree_iter->GetActureTreeNode();
        }
    }
    return n;
}

template<typename T>
double RandomForest::ORForest<T>::GetAverageOOBE() const
{
    double oobe=0;

    for (TreeListConstIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
    {
        oobe += tree_iter->GetOOBE(testData);
    }
    return oobe/treeNumber;
}

template<typename T>
void RandomForest::ORForest<T>::GetRankedGiniImportance( std::shared_ptr<std::vector<int> > * o_featureIndexList,  std::shared_ptr<std::vector<double> > * o_giniImportanceList)
{
    std::shared_ptr<std::vector<int> > featureIndexList=* o_featureIndexList;
    std::shared_ptr<std::vector<double> > giniImportanceList=* o_giniImportanceList;
    featureIndexList->resize(trainData->at(0)->size()-1);
    giniImportanceList->resize(trainData->at(0)->size()-1);
    for(int i=0;i<featureIndexList->size();i++)
    {
        featureIndexList->at(i)=i;
        giniImportanceList->at(i)=0.0;
    }
    for (TreeListIterator tree_iter = trees.begin(); tree_iter != trees.end(); tree_iter++)
    {
        tree_iter->UpdateGiniImportance();
        for(int j=0;j<featureIndexList->size();j++)
        {
            giniImportanceList->at(j) = giniImportanceList->at(j) + tree_iter->GetGiniImportance()->at(j);
        }
    }
    
    // sorting
    for(int i=0;i<featureIndexList->size()-1;i++)
    {
        for(int j=i+1;j<featureIndexList->size();j++)
        {
            double leftValue=giniImportanceList->at(i);
            double rightValue=giniImportanceList->at(j);
            if(leftValue<rightValue)
            {
                int leftIndex=featureIndexList->at(i);
                int rightIndex=featureIndexList->at(j);
                giniImportanceList->at(i)=rightValue;
                giniImportanceList->at(j)=leftValue;
                featureIndexList->at(i)=rightIndex;
                featureIndexList->at(j)=leftIndex;
            }
        }
    }
    
    *o_featureIndexList=featureIndexList;
    *o_giniImportanceList=giniImportanceList;
}

template class RandomForest::ORForest<float>;
template class RandomForest::ORForest<double>;
