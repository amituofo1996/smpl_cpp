#include <iostream>
using namespace std;

// python code/fit_3d.py $PWD --viz
// cd code;
// python smpl_webuser/hello_world/render_smpl.py

#include <eigen3/unsupported/Eigen/CXX11/Tensor>
#include <renderer.hpp>
#include <eigen3/Eigen/Eigen>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <opencv2/opencv.hpp>
#include <opencv2/core/eigen.hpp>
#include <iomanip>
#include <chrono>

#include <tensor.h>

void printEigen(const Eigen::MatrixXf& m, int rowCount = 4, int colCount = 4){
    std::cout << std::setprecision(3) << std::fixed;
    cout << "R: " << m.rows() << " C: " << m.cols() << endl;
    cout << "[";
    for(int i=0; i<m.rows(); i++){
        if(i < rowCount){
            cout << "[";
            for(int j=0; j<m.cols(); j++){
                if(j < colCount)
                    cout << m(i,j) << "     ";
                else if(j == colCount)
                    cout << " ... ";
                else if(j >= m.cols()-colCount)
                    cout << m(i,j) << "     ";
            }
            cout << "]" << endl;
        }else if(i == rowCount){
            cout << "..." << endl;
            cout << "..." << endl;
        }else if(i >= m.rows()-rowCount){
            cout << "[";
            for(int j=0; j<m.cols(); j++){
                if(j < colCount)
                    cout << m(i,j) << "     ";
                else if(j == colCount)
                    cout << " ... ";
                else if(j >= m.cols()-colCount)
                    cout << m(i,j) << "     ";
            }
            cout << "]" << endl;
        }
    }
    cout << "]" << endl;
}

Eigen::MatrixXf getMatrixFromTensor(const Eigen::Tensor<float,3>& t, int d){
    if(d >= t.dimensions()[0]){
        throw std::runtime_error("Wrong d");
    }
    Eigen::MatrixXf m;
    m.resize(t.dimensions()[1],t.dimensions()[2]);
    for(int r=0;r<t.dimensions()[1]; r++){
        for(int c=0;c<t.dimensions()[2]; c++){
            m(r,c) = t(d,r,c);
        }
    }
    return m;
}

Eigen::MatrixXf getMatrixFromTensor(const Eigen::Tensor<float,2>& t){
    Eigen::MatrixXf m;
    m.resize(t.dimensions()[0],t.dimensions()[1]);
    for(int r=0;r<t.dimensions()[0]; r++){
        for(int c=0;c<t.dimensions()[1]; c++){
            m(r,c) = t(r,c);
        }
    }
    return m;
}

Eigen::Tensor<float, 2> getTensorFromMatrix(const Eigen::MatrixXf& m){
    Eigen::Tensor<float,2> t(1,1);
    std::array<Eigen::Tensor<float, 2>::Index, 2> size = {m.rows(),m.cols()};
    t.resize(size);
    for(int r=0;r<m.rows(); r++){
        for(int c=0;c<m.cols(); c++){
            t(r,c) = m(r,c);
        }
    }
    return t;
}

void printTensor(Eigen::Tensor<float,3>& t, int dCount = 4, int rowCount = 4, int colCount = 4){
    cout << endl;
    cout << "D: " << t.dimensions()[0] << " ";
    for(int d=0;d<t.dimensions()[0]; d++){
        if(d < dCount || d >= t.dimensions()[0]-dCount){
            printEigen(getMatrixFromTensor(t,d), rowCount, colCount);
        }else if(d==dCount){
            cout << "..." << endl;
            cout << "..." << endl;
        }
    }
    cout << endl;
}

void printTensor(Eigen::Tensor<float,2>& t, int rowCount = 4, int colCount = 4){
    cout << endl;
    printEigen(getMatrixFromTensor(t), rowCount, colCount);
    cout << endl;
}


class SMPL{
public:
    Eigen::MatrixXf mV, mVTemp1, mVTemp2;
    Eigen::MatrixXf mF;
    Eigen::MatrixXf mPose;
    Eigen::MatrixXf mKintreeTable;
    Eigen::MatrixXf mJ;
    Eigen::MatrixXf mTrans;
    Eigen::MatrixXf mWeights;
    Eigen::MatrixXf mWeightsT;
    Eigen::MatrixXf vertSymIdxs;
    Eigen::MatrixXf mBetas;
    typedef Eigen::Matrix<float, 4, 24> BlockMatrix;
    std::vector<Eigen::MatrixXf> weightedBlockMatrix1;
    std::vector<BlockMatrix> blocks;
    std::vector<Eigen::MatrixXf> mShapeDirs;

    Eigen::Tensor<float, 3> mShapedDirsTensor;

    SMPL(){
        blocks.resize(4);
        weightedBlockMatrix1.resize(4);
        mBetas.resize(10,1);
        mBetas.setZero();
    }

    bool loadTensorFromJSON(const Json::Value& json, Eigen::Tensor<float, 3>& t, bool debug = false){
        int depth = json.size();
        int rows = json[0].size();
        int cols = json[0][0].size();
        if(debug){
            cout << "D: " << depth;
            cout << " R: " << rows;
            cout << " C: " << cols << endl;
        }

        std::array<Eigen::Tensor<float, 3>::Index, 3> size = {depth,rows,cols};
        t.resize(size);

        for(int d=0; d<depth; d++){
            for(int r=0; r<rows; r++){
                for(int c=0; c<cols; c++){
                    t(d,r,c) = json[d][r][c].asFloat();
                }
            }
        }

        return true;
    }

    bool loadEigenVecFromJSON(const Json::Value& json, std::vector<Eigen::MatrixXf>& t, bool debug = false){
        int depth = json.size();
        int rows = json[0].size();
        int cols = json[0][0].size();
        if(debug){
            cout << "D: " << depth;
            cout << " R: " << rows;
            cout << " C: " << cols << endl;
        }

        t.resize(depth);
        for(Eigen::MatrixXf& m : t){
            m.resize(rows, cols);
        }

        for(int d=0; d<depth; d++){
            Eigen::MatrixXf& m = t[d];
            for(int r=0; r<rows; r++){
                for(int c=0; c<cols; c++){
                    t[d](r,c) = json[d][r][c].asFloat();
                }
            }
        }

        return true;
    }

    bool loadEigenFromJSON(const Json::Value& json, Eigen::MatrixXf& m, bool debug = false){
        // Set Shape
        int rows = json.size();
        if(!rows) { cerr << "Matrix Has no Rows" << endl; return false;}
        int cols = json[0].size();
        if(rows == 0) rows = 1;
        m.resize(rows, cols);

        // Load Data
        if(rows > 1){
            for(int i=0; i<rows; i++){
                for(int j=0; j<cols; j++){
                    m(i,j) = json[i][j].asFloat();
                }
            }
        }else{
            throw std::runtime_error("Something wrong");
        }

        return true;
    }

    bool loadModelFromJSONFile(std::string filePath){
        ifstream in(filePath);
        Json::Value root;
        in >> root;
        if(!root.size()){
            cerr << "Failed to load model file" << endl;
            return false;
        }

        cout << root["pose_training_info"] << endl;

        loadEigenFromJSON(root["pose"], mPose);

        loadEigenVecFromJSON(root["shapedirs"], mShapeDirs);

        loadTensorFromJSON(root["shapedirs"], mShapedDirsTensor);

        loadEigenFromJSON(root["f"], mF);

        loadEigenFromJSON(root["kintree_table"], mKintreeTable);

        loadEigenFromJSON(root["J"], mJ);

        loadEigenFromJSON(root["trans"], mTrans);

        loadEigenFromJSON(root["v_posed"], mV);
        mV.conservativeResize(mV.rows(),mV.cols()+1);
        mV.col(mV.cols()-1) = Eigen::VectorXf::Ones(mV.rows());
        mVTemp1 = mV;
        mVTemp2 = mV;
        for(Eigen::MatrixXf& w : weightedBlockMatrix1) w.resize(4,mV.rows());

        loadEigenFromJSON(root["weights"], mWeights);
        mWeightsT = mWeights.transpose();

        loadEigenFromJSON(root["vert_sym_idxs"], vertSymIdxs);

        return true;
    }

    Eigen::Matrix4f rod(const Eigen::Vector3f& v, const Eigen::Vector3f& t){
        Eigen::Matrix4f m;
        cv::Mat src(cv::Size(1,3),CV_32FC1,cv::Scalar(0));
        src.at<float>(0) = v(0);
        src.at<float>(1) = v(1);
        src.at<float>(2) = v(2);
        cv::Mat dst;
        cv::Rodrigues(src, dst);
        m(0,0) = dst.at<float>(0,0);
        m(0,1) = dst.at<float>(0,1);
        m(0,2) = dst.at<float>(0,2);
        m(0,3) = t(0);
        m(1,0) = dst.at<float>(1,0);
        m(1,1) = dst.at<float>(1,1);
        m(1,2) = dst.at<float>(1,2);
        m(1,3) = t(1);
        m(2,0) = dst.at<float>(2,0);
        m(2,1) = dst.at<float>(2,1);
        m(2,2) = dst.at<float>(2,2);
        m(2,3) = t(2);
        m(3,0) = 0;
        m(3,1) = 0;
        m(3,2) = 0;
        m(3,3) = 1;
        return m;
    }

    bool updateModel(){

        // Create parent link table
        // {1: 0, 2: 0, 3: 0, 4: 1, 5: 2, 6: 3, 7: 4, 8: 5, 9: 6, 10: 7, 11: 8, 12: 9, 13: 9, 14: 9, 15: 12, 16: 13, 17: 14, 18: 16, 19: 17, 20: 18, 21: 19, 22: 20, 23: 21}
        std::map<int,int> parent;
        for(int i=1; i<mKintreeTable.cols(); i++){
            int key = mKintreeTable(1,i); int val = mKintreeTable(0,i);
            parent[key] = val;
        }

        std::vector<Eigen::Matrix4f> globalTransforms(24);
        std::vector<Eigen::Matrix4f> transforms(24);

        //        // Shape
        //        Eigen::Tensor<float, 2> mBetasTensor = getTensorFromMatrix(mBetas);
        //        Eigen::array<Eigen::IndexPair<int>, 1> product_dims = { Eigen::IndexPair<int>(2, 0) };
        //        Eigen::Tensor<float, 3> AB = mShapedDirsTensor.contract(mBetasTensor, product_dims);
        //        for(int i=0; i<mVTemp1.rows(); i++){
        //            mVTemp1(i,0) = mV(i,0) + AB(i,0,0);
        //            mVTemp1(i,1) = mV(i,1) + AB(i,1,0);
        //            mVTemp1(i,2) = mV(i,2) + AB(i,2,0);
        //            mVTemp1(i,3) = 1;
        //        }

        // Shape
        for(int i=0; i<mShapeDirs.size(); i++){
            mVTemp1.row(i) = mV.row(i) + (mShapeDirs[i] * mBetas).transpose();
            mVTemp1(i,3) = 1;
        }

        // Body pose
        Eigen::Matrix4f& bodyPose = globalTransforms[0];
        bodyPose = rod(mPose.row(0), mJ.row(0));

        // Global Transforms
        for(int i=1; i<globalTransforms.size(); i++){
            Eigen::Matrix4f& pose = globalTransforms[i];
            pose = globalTransforms[parent[i]] * rod(mPose.row(i), mJ.row(i) - mJ.row(parent[i]));
        }

        // Transforms
        for(int i=0; i<transforms.size(); i++){
            Eigen::Matrix4f& pose = transforms[i];
            Eigen::Vector4f jZero;
            jZero << mJ(i,0), mJ(i,1), mJ(i,2), 0;
            Eigen::Vector4f fx = globalTransforms[i] * jZero; // Only apply rot to jVector
            Eigen::Matrix4f pack = Eigen::Matrix4f::Zero();
            pack(0,3) = fx(0);
            pack(1,3) = fx(1);
            pack(2,3) = fx(2);
            pose = globalTransforms[i] - pack; // Only minus t component from transform with rotated jVector
        }

        // Generate transform from weights
        for(int b=0; b<4; b++){
            BlockMatrix& block = blocks[b];
            for(int i=0; i<24; i++){
                block.col(i) = transforms[i].row(b);
            }
            weightedBlockMatrix1[b] = block*mWeightsT; // Column x VSize ~2ms
        }

        // Transform vertices with weight matrix
        for(int b=0; b<4; b++){
            Eigen::MatrixXf& block = weightedBlockMatrix1[b];
            for(int i=0; i<mV.rows(); i++){
                mVTemp2(i,b) = mVTemp1.row(i) * block.col(i);
            }
        }

        return true;
    }
};



void testTensor(){
    //    Eigen::Tensor<float, 3> a(1000,3,10);
    //    Eigen::Tensor<float, 3> b(10,3);

    TensorD<3> tensorA;
    tensorA.resize({3,3,3});
    tensorA.setZero();
    tensorA.printSize();
    tensorA.setValues({{{1,2,3},
                        {4,5,6},
                        {7,8,9}},

                       {{10,11,12},
                        {13,14,15},
                        {16,17,18}},

                       {{19,20,21},
                        {22,23,24},
                        {25,26,27}}});
    tensorA.print();

    TensorD<2> tensorB;
    tensorB.resize({3,2});
    tensorB.printSize();
    tensorB.setValues({{1,3},
                       {2,3},
                       {3,3}});

    tensorB.print();

    tensorA.dot(tensorB).print();



    Eigen::Tensor<float, 3> xx(3,3,3);

    TensorD<3> ff = xx;

//    ff.printSize();


//    Eigen::Tensor<float, 3> eigenVersion;
//    Eigen::Tensor<float, 3>* ptr = &eigenVersion;
//    TensorD<3> myVersion = *ptr;

//    TensorD<3> myVersion;

//    TensorD<3>* ptr = &myVersion;
//    Eigen::Tensor<float, 3>& aa = *ptr;



//    TensorD<3> B;
//    B.setZero();
//    //B = B+B;

    //Tensor<3> out = tensorA.dot(tensorB);

//    Eigen::Tensor<float, 3> A;
//    Tensor<3> B;
//    B = B+B;
//    //A = B;


//    // Define Tensor
//    Eigen::Tensor<float, 3> tensorA(1,1,1);
//    // Resize it
//    std::array<Eigen::Tensor<float, 3>::Index, 3> size = {3,3,3};
//    tensorA.resize(size);
//    tensorA.setZero();
//    tensorA.setRandom();
//    tensorA.setValues({{{1,2,3},
//                        {4,5,6},
//                        {7,8,9}},

//                       {{10,11,12},
//                        {13,14,15},
//                        {16,17,18}},

//                       {{19,20,21},
//                        {22,23,24},
//                        {25,26,27}}});
//    cout << tensorA << endl;
//    cout << tensorA(1,2,2) << endl;
//    cout << tensorA.dimensions()[0] << endl;

//    cout <<  "***********" << endl;
//    Eigen::Tensor<float, 2> tensorB(3,2);
//    tensorB.setZero();
//    tensorB.setValues({{1,3},
//                       {2,3},
//                       {3,3}});
//    printEigen(getMatrixFromTensor(tensorB));

//    // Compute the traditional matrix product
//    Eigen::array<Eigen::IndexPair<int>, 1> product_dims = { Eigen::IndexPair<int>(2, 0) };
//    Eigen::Tensor<float, 3> AB = tensorA.contract(tensorB, product_dims);
//    printTensor(AB);

}

int main(int argc, char *argv[])
{
    testTensor();
    exit(-1);

    SMPL smpl;
    smpl.loadModelFromJSONFile("/home/ryaadhav/smpl_cpp/model.json");
    std::cout.setstate(std::ios_base::failbit);
    smpl.updateModel();
    std::cout.clear();

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    smpl.updateModel();
    std::chrono::steady_clock::time_point end= std::chrono::steady_clock::now();
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()/1000. << " ms" << std::endl;

    bool active = false;
    op::WRender3D render;
    render.initializationOnThread();
    std::shared_ptr<op::WObject> wObject1 = std::make_shared<op::WObject>();
    //wObject1->loadOBJFile("/home/raaj/project/","hello_smpl.obj","");
    wObject1->loadEigenData(smpl.mVTemp2, smpl.mF);
    wObject1->print();
    render.addObject(wObject1);
    bool sw = true;
    while(1){

        if(active){
            static float currAng = 0.;
            static float currB = 0.;
            if(currAng >= 45) sw = false;
            else if(currAng <= -45) sw = true;
            if(sw) {
                currAng += 0.5;
                currB += 0.1;
            }
            else {
                currAng -= 0.5;
                currB -= 0.1;
            }
            smpl.mPose(1,0) = (M_PI/180. * currAng);
            smpl.mPose(1,1) = (M_PI/180. * currAng);
            smpl.mPose(15,2) = (M_PI/180. * currAng);
            smpl.mBetas(3) = currB;

            begin = std::chrono::steady_clock::now();
            smpl.updateModel();
            wObject1->loadEigenData(smpl.mVTemp2, smpl.mF);
            wObject1->rebuild(op::WObject::RENDER_NORMAL);
            end= std::chrono::steady_clock::now();
            std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() <<std::endl;
        }

        render.workOnThread();
    }
}


//Eigen::Tensor<float, 3> m(3,10,10);
////Eigen::Tensor<float, 3>::Index i;
//std::array<Eigen::Tensor<float, 3>::Index, 3> size = {3,3,3};
//m.resize(size);
//m.setZero();
//m(1,0,0) = 4;

//m(2,0,0) = 4;

//cout << m << endl;


////m.base().resize(2,2);
//// ...
////tensr.resize(...);

//exit(-1);
////Eigen::Tensor::Dimensions d = m.Dimensions;
