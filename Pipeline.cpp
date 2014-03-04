#include <string.h>
#include <math.h>
#include <iostream>
#include <stdexcept>
#include "FireLog.h"
#include "FireSight.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "jansson.h"
#include "jo_util.hpp"
#include "MatUtil.hpp"
#include "version.h"

using namespace cv;
using namespace std;
using namespace firesight;

StageData::StageData(string stageName) {
	LOGTRACE1("StageData constructor %s", stageName.c_str());
}

StageData::~StageData() {
	LOGTRACE("StageData destructor");
}

bool Pipeline::stageOK(const char *fmt, const char *errMsg, json_t *pStage, json_t *pStageModel) {
	if (errMsg) {
		char *pStageJson = json_dumps(pStage, JSON_COMPACT|JSON_PRESERVE_ORDER);
		LOGERROR2(fmt, pStageJson, errMsg);
		free(pStageJson);
		return false;
	}

	if (logLevel >= FIRELOG_TRACE) {
	  char *pStageJson = json_dumps(pStage, 0);
		char *pModelJson = json_dumps(pStageModel, 0);
		LOGTRACE2(fmt, pStageJson, pModelJson);
		free(pStageJson);
		free(pModelJson);
	}

	return true;
}

bool Pipeline::apply_warpAffine(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	const char *errMsg = NULL;
  float scale = jo_float(pStage, "scale", 1, model.argMap);
  float angle = jo_float(pStage, "angle", 0, model.argMap);
  int dx = jo_int(pStage, "dx", (int)((scale-1)*model.image.cols/2.0+.5), model.argMap);
  int dy = jo_int(pStage, "dy", (int)((scale-1)*model.image.rows/2.0+.5), model.argMap);
	string  borderModeStr = jo_string(pStage, "borderMode", "BORDER_REPLICATE", model.argMap);
	int borderMode;

	if (!errMsg) {
		if (borderModeStr.compare("BORDER_CONSTANT") == 0) {
			borderMode = BORDER_CONSTANT;
		} else if (borderModeStr.compare("BORDER_REPLICATE") == 0) {
			borderMode = BORDER_REPLICATE;
		} else if (borderModeStr.compare("BORDER_REFLECT") == 0) {
			borderMode = BORDER_REFLECT;
		} else if (borderModeStr.compare("BORDER_REFLECT_101") == 0) {
			borderMode = BORDER_REFLECT_101;
		} else if (borderModeStr.compare("BORDER_REFLECT101") == 0) {
			borderMode = BORDER_REFLECT101;
		} else if (borderModeStr.compare("BORDER_WRAP") == 0) {
			borderMode = BORDER_WRAP;
		} else {
			errMsg = "Expected borderMode: BORDER_CONSTANT, BORDER_REPLICATE, BORDER_REFLECT, BORDER_REFLECT_101, BORDER_WRAP";
		}
	}

	if (scale <= 0) {
		errMsg = "Expected 0<scale";
	}
	int width = jo_int(pStage, "width", model.image.cols, model.argMap);
	int height = jo_int(pStage, "height", model.image.rows, model.argMap);
	int cx = jo_int(pStage, "cx", (int)(0.5+model.image.cols/2.0), model.argMap);
	int cy = jo_int(pStage, "cy", (int)(0.5+model.image.rows/2.0), model.argMap);
	Scalar borderValue = jo_Scalar(pStage, "borderValue", Scalar::all(0), model.argMap);

	if (!errMsg) {
		Mat result;
		matWarpAffine(model.image, result, Point(cx,cy), angle, scale, Point(dx,dy), Size(width,height), borderMode, borderValue);
		model.image = result;
	}

	return stageOK("apply_warpAffine(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_putText(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	string text = jo_string(pStage, "text", "FireSight");
	Scalar color = jo_Scalar(pStage, "color", Scalar(0,255,0), model.argMap);
	string fontFaceName = jo_string(pStage, "fontFace", "FONT_HERSHEY_PLAIN");
	int thickness = jo_int(pStage, "thickness", 1);
	int fontFace = FONT_HERSHEY_PLAIN;
	double fontScale = jo_double(pStage, "fontScale", 1);
	Point org = jo_Point(pStage, "org", Point(5,model.image.rows-6));
	const char *errMsg = NULL;

	if (fontFaceName.compare("FONT_HERSHEY_SIMPLEX") == 0) {
		fontFace = FONT_HERSHEY_SIMPLEX;
	} else if (fontFaceName.compare("FONT_HERSHEY_PLAIN") == 0) {
		fontFace = FONT_HERSHEY_PLAIN;
	} else if (fontFaceName.compare("FONT_HERSHEY_COMPLEX") == 0) {
		fontFace = FONT_HERSHEY_COMPLEX;
	} else if (fontFaceName.compare("FONT_HERSHEY_DUPLEX") == 0) {
		fontFace = FONT_HERSHEY_DUPLEX;
	} else if (fontFaceName.compare("FONT_HERSHEY_TRIPLEX") == 0) {
		fontFace = FONT_HERSHEY_TRIPLEX;
	} else if (fontFaceName.compare("FONT_HERSHEY_COMPLEX_SMALL") == 0) {
		fontFace = FONT_HERSHEY_COMPLEX_SMALL;
	} else if (fontFaceName.compare("FONT_HERSHEY_SCRIPT_SIMPLEX") == 0) {
		fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
	} else if (fontFaceName.compare("FONT_HERSHEY_SCRIPT_COMPLEX") == 0) {
		fontFace = FONT_HERSHEY_SCRIPT_COMPLEX;
	} else {
		errMsg = "Unknown fontFace (default is FONT_HERSHEY_PLAIN)";
	}

	if (!errMsg) { 
		putText(model.image, text.c_str(), org, fontFace, fontScale, color, thickness);
	}

	return stageOK("apply_putText(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_resize(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
  double fx = jo_float(pStage, "fx", 1, model.argMap);
  double fy = jo_float(pStage, "fy", 1, model.argMap);
	const char *errMsg = NULL;

	if (fx <= 0 || fy <= 0) {
		errMsg = "Expected 0<fx and 0<fy";
	}
	if (!errMsg) { 
		Mat result;
		resize(model.image, result, Size(), fx, fy, INTER_AREA);
		model.image = result;
	}

	return stageOK("apply_resize(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_stageImage(json_t *pStage, json_t *pStageModel, Model &model) {
  string stageStr = jo_string(pStage, "stage", "", model.argMap);
	const char *errMsg = NULL;

	if (stageStr.empty()) {
		errMsg = "Expected name of stage for image";
	} else {
		model.image = model.imageMap[stageStr.c_str()];
		if (!model.image.rows || !model.image.cols) {
			model.image = model.imageMap["input"].clone();
			LOGTRACE1("Could not locate stage image '%s', using input image", stageStr.c_str());
		}
	}

	return stageOK("apply_stageImage(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_imread(json_t *pStage, json_t *pStageModel, Model &model) {
  string path = jo_string(pStage, "path", "", model.argMap);
	const char *errMsg = NULL;

	if (path.empty()) {
		errMsg = "expected path for imread";
	} else {
		model.image = imread(path.c_str(), CV_LOAD_IMAGE_COLOR);
		if (model.image.data) {
			json_object_set(pStageModel, "rows", json_integer(model.image.rows));
			json_object_set(pStageModel, "cols", json_integer(model.image.cols));
		} else {
			errMsg = "imread failed";
			cout << "ERROR:" << errMsg << endl;
		}
	}

	return stageOK("apply_imread(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_imwrite(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
  string path = jo_string(pStage, "path");
	const char *errMsg = NULL;

	if (path.empty()) {
		errMsg = "Expected path for imwrite";
	} else {
		bool result = imwrite(path.c_str(), model.image);
		json_object_set(pStageModel, "result", json_boolean(result));
	}

	return stageOK("apply_imwrite(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_cvtColor(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
  string codeStr = jo_string(pStage, "code", "CV_BGR2GRAY", model.argMap);
	int dstCn = jo_int(pStage, "dstCn", 0);
	const char *errMsg = NULL;
	int code = CV_BGR2GRAY;

	if (codeStr.compare("CV_RGB2GRAY")==0) {
	  code = CV_RGB2GRAY;
	} else if (codeStr.compare("CV_BGR2GRAY")==0) {
	  code = CV_BGR2GRAY;
	} else if (codeStr.compare("CV_GRAY2BGR")==0) {
	  code = CV_GRAY2BGR;
	} else if (codeStr.compare("CV_GRAY2RGB")==0) {
	  code = CV_GRAY2RGB;
	} else {
	  errMsg = "code unsupported";
	}
	if (dstCn < 0) {
		errMsg = "expected 0<dstCn";
	}

	if (!errMsg) {
		cvtColor(model.image, model.image, code, dstCn);
	}

	return stageOK("apply_cvtColor(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_drawRects(json_t *pStage, json_t *pStageModel, Model &model) {
	const char *errMsg = NULL;
	Scalar color = jo_Scalar(pStage, "color", Scalar::all(-1), model.argMap);
	int thickness = jo_int(pStage, "thickness", 2, model.argMap);
	string rectsModelName = jo_string(pStage, "model", "", model.argMap);
	json_t *pRectsModel = jo_object(model.getJson(false), rectsModelName.c_str(), model.argMap);

	if (!json_is_object(pRectsModel)) {
		errMsg = "Expected name of stage model with rects";
	}

	json_t *pRects = NULL;
	if (!errMsg) {
		pRects = jo_object(pRectsModel, "rects", model.argMap);
		if (!json_is_array(pRects)) {
			errMsg = "Expected array of rects";
		}
	}

	if (!errMsg) {
		if (model.image.channels() == 1) {
			LOGTRACE("Converting grayscale image to color image");
			cvtColor(model.image, model.image, CV_GRAY2BGR, 0);
		}
		size_t index;
		json_t *pRect;
		Point2f vertices[4];
		int blue = (int)color[0];
		int green = (int)color[1];
		int red = (int)color[2];
		bool changeColor = red == -1 && green == -1 && blue == -1;

		json_array_foreach(pRects, index, pRect) {
			int x = jo_int(pRect, "x", SHRT_MAX, model.argMap);
			int y = jo_int(pRect, "y", SHRT_MAX, model.argMap);
			int width = jo_int(pRect, "width", -1, model.argMap);
			int height = jo_int(pRect, "height", -1, model.argMap);
			float angle = jo_float(pRect, "angle", FLT_MAX, model.argMap);
			if (changeColor) {
				red = (index & 1) ? 0 : 255;
				green = (index & 2) ? 128 : 192;
				blue = (index & 1) ? 255 : 0;
				color = Scalar(blue, green, red);
			}
			if (x == SHRT_MAX || y == SHRT_MAX || width == SHRT_MAX || height == SHRT_MAX) {
				LOGERROR("apply_drawRects() x, y, width, height are required values");
				break;
			}
			if (angle == FLT_MAX) {
				circle(model.image, Point(x,y), (int)(0.5+min(width,height)/2.0), color, thickness);
			} else {
				RotatedRect rect(Point(x,y), Size(width, height), angle);
				rect.points(vertices);
				for (int i = 0; i < 4; i++) {
					line(model.image, vertices[i], vertices[(i+1)%4], color, thickness);
				}
			}
		}
	}

	return stageOK("apply_drawRects(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_drawKeypoints(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	const char *errMsg = NULL;
	Scalar color = jo_Scalar(pStage, "color", Scalar::all(-1), model.argMap);
	int flags = jo_int(pStage, "flags", DrawMatchesFlags::DRAW_OVER_OUTIMG|DrawMatchesFlags::DRAW_RICH_KEYPOINTS, model.argMap);
	string modelName = jo_string(pStage, "model", "", model.argMap);
	json_t *pKeypointStage = jo_object(model.getJson(false), modelName.c_str(), model.argMap);

	if (!pKeypointStage) {
		string keypointStageName = jo_string(pStage, "keypointStage", "", model.argMap);
		pKeypointStage = jo_object(model.getJson(false), keypointStageName.c_str(), model.argMap);
	}

	if (!errMsg && flags < 0 || 7 < flags) {
		errMsg = "expected 0 < flags < 7";
	}

	if (!errMsg && !pKeypointStage) {
		errMsg = "expected name of stage model";
	}

	vector<KeyPoint> keypoints;
	if (!errMsg) {
		json_t *pKeypoints = jo_object(pKeypointStage, "keypoints", model.argMap);
		if (!json_is_array(pKeypoints)) {
		  errMsg = "keypointStage has no keypoints JSON array";
		} else {
			size_t index;
			json_t *pKeypoint;
			json_array_foreach(pKeypoints, index, pKeypoint) {
				float x = jo_float(pKeypoint, "pt.x", -1, model.argMap);
				float y = jo_float(pKeypoint, "pt.y", -1, model.argMap);
				float size = jo_float(pKeypoint, "size", 10, model.argMap);
				float angle = jo_float(pKeypoint, "angle", -1, model.argMap);
				KeyPoint keypoint(x, y, size, angle);
				keypoints.push_back(keypoint);
			}
		}
	}

	if (!errMsg) {
		drawKeypoints(model.image, keypoints, model.image, color, flags);
	}

	return stageOK("apply_drawKeypoints(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_dilate(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	const char *errMsg = NULL;
	int kwidth = jo_int(pStage, "ksize.width", 3, model.argMap);
	int kheight = jo_int(pStage, "ksize.height", 3, model.argMap);
	int shape = jo_shape(pStage, "shape", errMsg);

	if (!errMsg) {
	  Mat structuringElement = getStructuringElement(shape, Size(kwidth, kheight));
		dilate(model.image, model.image, structuringElement);
	}

	return stageOK("apply_dilate(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_erode(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	const char *errMsg = NULL;
	int kwidth = jo_int(pStage, "ksize.width", 3, model.argMap);
	int kheight = jo_int(pStage, "ksize.height", 3, model.argMap);
	int shape = jo_shape(pStage, "shape", errMsg);

	if (!errMsg) {
	  Mat structuringElement = getStructuringElement(shape, Size(kwidth, kheight));
		erode(model.image, model.image, structuringElement);
	}

	return stageOK("apply_erode(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_equalizeHist(json_t *pStage, json_t *pStageModel, Model &model) {
	const char *errMsg = NULL;

	if (!errMsg) {
		equalizeHist(model.image, model.image);
	}

	return stageOK("apply_equalizeHist(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_blur(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	const char *errMsg = NULL;
	int width = jo_int(pStage, "ksize.width", 3, model.argMap);
	int height = jo_int(pStage, "ksize.height", 3, model.argMap);
	int anchorx = jo_int(pStage, "anchor.x", -1, model.argMap);
	int anchory = jo_int(pStage, "anchor.y", -1, model.argMap);

	if (width <= 0 || height <= 0) {
		errMsg = "expected 0<width and 0<height";
	}

	if (!errMsg) {
		blur(model.image, model.image, Size(width,height));
	}

	return stageOK("apply_blur(%s) %s", errMsg, pStage, pStageModel);
}

static void modelKeyPoints(json_t*pStageModel, const vector<KeyPoint> &keyPoints) {
	json_t *pKeyPoints = json_array();
	json_object_set(pStageModel, "keypoints", pKeyPoints);
	for (size_t i=0; i<keyPoints.size(); i++){
	  json_t *pKeyPoint = json_object();
		json_object_set(pKeyPoint, "pt.x", json_real(keyPoints[i].pt.x));
		json_object_set(pKeyPoint, "pt.y", json_real(keyPoints[i].pt.y));
		json_object_set(pKeyPoint, "size", json_real(keyPoints[i].size));
		if (keyPoints[i].angle != -1) {
			json_object_set(pKeyPoint, "angle", json_real(keyPoints[i].angle));
		}
		if (keyPoints[i].response != 0) {
			json_object_set(pKeyPoint, "response", json_real(keyPoints[i].response));
		}
		json_array_append(pKeyPoints, pKeyPoint);
	}
}

bool Pipeline::apply_SimpleBlobDetector(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	SimpleBlobDetector::Params params;
	params.thresholdStep = jo_float(pStage, "thresholdStep", params.thresholdStep, model.argMap);
	params.minThreshold = jo_float(pStage, "minThreshold", params.minThreshold, model.argMap);
	params.maxThreshold = jo_float(pStage, "maxThreshold", params.maxThreshold, model.argMap);
	params.minRepeatability = jo_int(pStage, "minRepeatability", params.minRepeatability, model.argMap);
	params.minDistBetweenBlobs = jo_float(pStage, "minDistBetweenBlobs", params.minDistBetweenBlobs, model.argMap);
	params.filterByColor = jo_bool(pStage, "filterByColor", params.filterByColor);
	params.blobColor = jo_int(pStage, "blobColor", params.blobColor, model.argMap);
	params.filterByArea = jo_bool(pStage, "filterByArea", params.filterByArea);
	params.minArea = jo_float(pStage, "minArea", params.minArea, model.argMap);
	params.maxArea = jo_float(pStage, "maxArea", params.maxArea, model.argMap);
	params.filterByCircularity = jo_bool(pStage, "filterByCircularity", params.filterByCircularity);
	params.minCircularity = jo_float(pStage, "minCircularity", params.minCircularity, model.argMap);
	params.maxCircularity = jo_float(pStage, "maxCircularity", params.maxCircularity, model.argMap);
	params.filterByInertia = jo_bool(pStage, "filterByInertia", params.filterByInertia, model.argMap);
	params.minInertiaRatio = jo_float(pStage, "minInertiaRatio", params.minInertiaRatio, model.argMap);
	params.maxInertiaRatio = jo_float(pStage, "maxInertiaRatio", params.maxInertiaRatio, model.argMap);
	params.filterByConvexity = jo_bool(pStage, "filterByConvexity", params.filterByConvexity);
	params.minConvexity = jo_float(pStage, "minConvexity", params.minConvexity, model.argMap);
	params.maxConvexity = jo_float(pStage, "maxConvexity", params.maxConvexity, model.argMap);
	const char *errMsg = NULL;

	if (!errMsg) {
		SimpleBlobDetector detector(params);
		SimpleBlobDetector(params);
		detector.create("SimpleBlob");
		vector<cv::KeyPoint> keyPoints;
	  LOGTRACE("apply_SimpleBlobDetector detect()");
		detector.detect(model.image, keyPoints);
		modelKeyPoints(pStageModel, keyPoints);
	}

	return stageOK("apply_SimpleBlobDetector(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_rectangle(json_t *pStage, json_t *pStageModel, Model &model) {
	int x = jo_int(pStage, "x", 0, model.argMap);
	int y = jo_int(pStage, "y", 0, model.argMap);
	int width = jo_int(pStage, "width", model.image.cols, model.argMap);
	int height = jo_int(pStage, "height", model.image.rows, model.argMap);
	int thickness = jo_int(pStage, "thickness", 1, model.argMap);
	int lineType = jo_int(pStage, "lineType", 8, model.argMap);
	Scalar color = jo_Scalar(pStage, "color", Scalar::all(0), model.argMap);
	Scalar flood = jo_Scalar(pStage, "flood", Scalar::all(-1), model.argMap);
	Scalar fill = jo_Scalar(pStage, "fill", Scalar::all(-1), model.argMap);
	int shift = jo_int(pStage, "shift", 0, model.argMap);
	const char *errMsg = NULL;

  if ( x < 0 || y < 0) {
		errMsg = "Expected 0<=x and 0<=y";
	} else if (shift < 0) {
		errMsg = "Expected shift>=0";
	}

	if (!errMsg) {
		if (model.image.cols == 0 || model.image.rows == 0) {
			model.image = Mat(height, width, CV_8UC3, Scalar(0,0,0));
		}
		if (thickness) {
			rectangle(model.image, Rect(x,y,width,height), color, thickness, lineType, shift);
		}
		if (thickness >= 0) {
			int outThickness = thickness/2;
			int inThickness = (int)(thickness - outThickness);
			if (fill[0] >= 0) {
				rectangle(model.image, Rect(x+inThickness,y+inThickness,width-inThickness*2,height-inThickness*2), fill, -1, lineType, shift);
			}
			if (flood[0] >= 0) {
				int left = x - outThickness;
				int top = y - outThickness;
				int right = x+width+outThickness;
				int bot = y+height+outThickness;
				rectangle(model.image, Rect(0,0,model.image.cols,top), flood, -1, lineType, shift);
				rectangle(model.image, Rect(0,bot,model.image.cols,model.image.rows-bot), flood, -1, lineType, shift);
				rectangle(model.image, Rect(0,top,left,height+outThickness*2), flood, -1, lineType, shift);
				rectangle(model.image, Rect(right,top,model.image.cols-right,height+outThickness*2), flood, -1, lineType, shift);
			}
		}
	}

	return stageOK("apply_rectangle(%s) %s", errMsg, pStage, pStageModel);
}

int Pipeline::parseCvType(const char *typeStr, const char *&errMsg) {
	int type = CV_8U;

	if (strcmp("CV_8UC3", typeStr) == 0) {
		type = CV_8UC3;
	} else if (strcmp("CV_8UC2", typeStr) == 0) {
		type = CV_8UC2;
	} else if (strcmp("CV_8UC1", typeStr) == 0) {
		type = CV_8UC1;
	} else if (strcmp("CV_8U", typeStr) == 0) {
		type = CV_8UC1;
	} else if (strcmp("CV_32F", typeStr) == 0) {
		type = CV_32F;
	} else if (strcmp("CV_32FC1", typeStr) == 0) {
		type = CV_32FC1;
	} else if (strcmp("CV_32FC2", typeStr) == 0) {
		type = CV_32FC2;
	} else if (strcmp("CV_32FC3", typeStr) == 0) {
		type = CV_32FC3;
	} else {
		errMsg = "Unsupported type";
	}

	return type;
}

bool Pipeline::apply_Mat(json_t *pStage, json_t *pStageModel, Model &model) {
	int width = jo_int(pStage, "width", model.image.cols, model.argMap);
	int height = jo_int(pStage, "height", model.image.rows, model.argMap);
	string typeStr = jo_string(pStage, "type", "CV_8UC3", model.argMap);
	Scalar color = jo_Scalar(pStage, "color", Scalar::all(0), model.argMap);
	const char *errMsg = NULL;
	int type = CV_8UC3;

	if (width <= 0 || height <= 0) {
		errMsg = "Expected 0<width and 0<height";
	} else if (color[0] <0 || color[1]<0 || color[2]<0) {
		errMsg = "Expected color JSON array with non-negative values";
	} 
	
	if (!errMsg) {
		type = parseCvType(typeStr.c_str(), errMsg);
	}

	if (!errMsg) {
		model.image = Mat(height, width, type, color);
	}

	return stageOK("apply_Mat(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_calcHist(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	float rangeMin = jo_float(pStage, "rangeMin", 0, model.argMap);
	float rangeMax = jo_float(pStage, "rangeMax", 256, model.argMap);
	int locations = jo_int(pStage, "locations", 0, model.argMap);
	int bins = jo_int(pStage, "bins", (int)(rangeMax-rangeMin), model.argMap);
	int histSize = bins;
	bool uniform = true;
	bool accumulate = false;
	const char *errMsg = NULL;
	Mat mask;

	if (rangeMin > rangeMax) {
		errMsg = "Expected rangeMin <= rangeMax";
	} else if (bins < 2 || bins > 256 ) {
		errMsg = "Expected 1<bins and bins<=256";
	}

	if (!errMsg) {
		float rangeC0[] = { rangeMin, rangeMax }; 
		const float* ranges[] = { rangeC0 };
		Mat hist;
		calcHist(&model.image, 1, 0, mask, hist, 1, &histSize, ranges, uniform, accumulate);
		json_t *pHist = json_array();
		for (int i = 0; i < histSize; i++) {
			json_array_append(pHist, json_real(hist.at<float>(i)));
		}
		json_object_set(pStageModel, "hist", pHist);
		json_t *pLocations = json_array();
		json_object_set(pStageModel, "locations", pLocations);
	}

	return stageOK("apply_calcHist(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_split(json_t *pStage, json_t *pStageModel, Model &model) {
	json_t *pFromTo = jo_object(pStage, "fromTo", model.argMap);
	const char *errMsg = NULL;
#define MAX_FROMTO 32
	int fromTo[MAX_FROMTO];
	int nFromTo;

	if (!json_is_array(pFromTo)) {
		errMsg = "Expected JSON array for fromTo";
	}

	if (!errMsg) {
		json_t *pInt;
		size_t index;
		json_array_foreach(pFromTo, index, pInt) {
			if (index >= MAX_FROMTO) {
				errMsg = "Too many channels";
				break;
			}
			nFromTo = index+1;
			fromTo[index] = (int)json_integer_value(pInt);
		}
	}

	if (!errMsg) {
		int depth = model.image.depth();
		int channels = 1;
		Mat outImage( model.image.rows, model.image.cols, CV_MAKETYPE(depth, channels) );
		LOGTRACE1("Creating output model.image %s", matInfo(outImage).c_str());
		Mat out[] = { outImage };
		mixChannels( &model.image, 1, out, 1, fromTo, nFromTo/2 );
		model.image = outImage;
	}

	return stageOK("apply_split(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_convertTo(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	double alpha = jo_float(pStage, "alpha", 1, model.argMap);
	double delta = jo_float(pStage, "delta", 0, model.argMap);
	string transform = jo_string(pStage, "transform", "", model.argMap);
	string rTypeStr = jo_string(pStage, "rType", "CV_8U", model.argMap);
	const char *errMsg = NULL;
	int rType;

	if (!errMsg) {
		rType = parseCvType(rTypeStr.c_str(), errMsg);
	}

	if (!transform.empty()) {
		if (transform.compare("log") == 0) {
			LOGTRACE("log()");
			log(model.image, model.image);
		}
	}
	
	if (!errMsg) {
		model.image.convertTo(model.image, rType, alpha, delta);
	}

	return stageOK("apply_convertTo(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_cout(json_t *pStage, json_t *pStageModel, Model &model) {
	int col = jo_int(pStage, "col", 0, model.argMap);
	int row = jo_int(pStage, "row", 0, model.argMap);
	int cols = jo_int(pStage, "cols", model.image.cols, model.argMap);
	int rows = jo_int(pStage, "rows", model.image.rows, model.argMap);
	int precision = jo_int(pStage, "precision", 1, model.argMap);
	int width = jo_int(pStage, "width", 5, model.argMap);
	int channel = jo_int(pStage, "channel", 0, model.argMap);
	string comment = jo_string(pStage, "comment", "", model.argMap);
	const char *errMsg = NULL;

	if (row<0 || col<0 || rows<=0 || cols<=0) {
		errMsg = "Expected 0<=row and 0<=col and 0<cols and 0<rows";
	}
	if (rows > model.image.rows) {
		rows = model.image.rows;
	}
	if (cols > model.image.cols) {
		cols = model.image.cols;
	}

	if (!errMsg) {
		int depth = model.image.depth();
		cout << matInfo(model.image);
		cout << " show:[" << row << "-" << row+rows-1 << "," << col << "-" << col+cols-1 << "]";
		if (comment.size()) {
			cout << " " << comment;
		}
		cout << endl;
		for (int r = row; r < row+rows; r++) {
			for (int c = col; c < col+cols; c++) {
				cout.precision(precision);
				cout.width(width);
				if (model.image.channels() == 1) {
					switch (depth) {
						case CV_8S:
						case CV_8U:
							cout << (short) model.image.at<unsigned char>(r,c,channel) << " ";
							break;
						case CV_16U:
							cout << model.image.at<unsigned short>(r,c) << " ";
							break;
						case CV_16S:
							cout << model.image.at<short>(r,c) << " ";
							break;
						case CV_32S:
							cout << model.image.at<int>(r,c) << " ";
							break;
						case CV_32F:
							cout << std::fixed;
							cout << model.image.at<float>(r,c) << " ";
							break;
						case CV_64F:
							cout << std::fixed;
							cout << model.image.at<double>(r,c) << " ";
							break;
						default:
							cout << "UNSUPPORTED-CONVERSION" << " ";
							break;
					}
				} else {
					switch (depth) {
						case CV_8S:
						case CV_8U:
							cout << (short) model.image.at<Vec2b>(r,c)[channel] << " ";
							break;
						case CV_16U:
							cout << model.image.at<Vec2w>(r,c)[channel] << " ";
							break;
						case CV_16S:
							cout << model.image.at<Vec2s>(r,c)[channel] << " ";
							break;
						case CV_32S:
							cout << model.image.at<Vec2i>(r,c)[channel] << " ";
							break;
						case CV_32F:
							cout << std::fixed;
							cout << model.image.at<Vec2f>(r,c)[channel] << " ";
							break;
						case CV_64F:
							cout << std::fixed;
							cout << model.image.at<Vec2d>(r,c)[channel] << " ";
							break;
						default:
							cout << "UNSUPPORTED-CONVERSION" << " ";
							break;
					}
				}
			}
			cout << endl;
		}
	}

	return stageOK("apply_cout(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_normalize(json_t *pStage, json_t *pStageModel, Model &model) {
	double alpha = jo_float(pStage, "alpha", 1, model.argMap);
	double beta = jo_float(pStage, "beta", 0, model.argMap);
	string normTypeStr = jo_string(pStage, "normType", "NORM_L2", model.argMap);
	int normType  = NORM_L2;
	const char *errMsg = NULL;

	if (normTypeStr.compare("NORM_L2") == 0) {
		normType = NORM_L2;
	} else if (normTypeStr.compare("NORM_L1") == 0) {
		normType = NORM_L1;
	} else if (normTypeStr.compare("NORM_MINMAX") == 0) {
		normType = NORM_MINMAX;
	} else if (normTypeStr.compare("NORM_INF") == 0) {
		normType = NORM_INF;
	} else {
		errMsg = "Unknown normType";
	}

	if (!errMsg) {
		normalize(model.image, model.image, alpha, beta, normType);
	}

	return stageOK("apply_normalize(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_Canny(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	float threshold1 = jo_float(pStage, "threshold1", 0, model.argMap);
	float threshold2 = jo_float(pStage, "threshold2", 50, model.argMap);
	int apertureSize = jo_int(pStage, "apertureSize", 3, model.argMap);
	bool L2gradient = jo_bool(pStage, "L2gradient", false);
	const char *errMsg = NULL;

	if (!errMsg) {
		Canny(model.image, model.image, threshold1, threshold2, apertureSize, L2gradient);
	}

	return stageOK("apply_Canny(%s) %s", errMsg, pStage, pStageModel);
}

bool Pipeline::apply_HoleRecognizer(json_t *pStage, json_t *pStageModel, Model &model) {
	validateImage(model.image);
	float diamMin = jo_float(pStage, "diamMin", 0, model.argMap);
	float diamMax = jo_float(pStage, "diamMax", 0, model.argMap);
	int showMatches = jo_int(pStage, "show", 0, model.argMap);
	const char *errMsg = NULL;

	if (diamMin <= 0 || diamMax <= 0 || diamMin > diamMax) {
		errMsg = "expected: 0 < diamMin < diamMax ";
	} else if (showMatches < 0) {
		errMsg = "expected: 0 < showMatches ";
	} else if (logLevel >= FIRELOG_TRACE) {
		char *pStageJson = json_dumps(pStage, 0);
		LOGTRACE1("apply_HoleRecognizer(%s)", pStageJson);
		free(pStageJson);
	}
	if (!errMsg) {
		vector<MatchedRegion> matches;
		HoleRecognizer recognizer(diamMin, diamMax);
		recognizer.showMatches(showMatches);
		recognizer.scan(model.image, matches);
		json_t *holes = json_array();
		json_object_set(pStageModel, "holes", holes);
		for (size_t i = 0; i < matches.size(); i++) {
			json_array_append(holes, matches[i].as_json_t());
		}
	}

	return stageOK("apply_HoleRecognizer(%s) %s", errMsg, pStage, pStageModel);
}

Pipeline::Pipeline(const char *pJson) {
	json_error_t jerr;
	pPipeline = json_loads(pJson, 0, &jerr);

	if (!pPipeline) {
		LOGERROR3("Pipeline::process cannot parse json: %s src:%s line:%d", jerr.text, jerr.source, jerr.line);
		throw jerr;
	}
}

Pipeline::Pipeline(json_t *pJson) {
  pPipeline = json_incref(pJson);
}

Pipeline::~Pipeline() {
	if (pPipeline->refcount == 1) {
		LOGTRACE1("~Pipeline() pPipeline->refcount:%d", pPipeline->refcount);
	} else {
		LOGERROR1("~Pipeline() pPipeline->refcount:%d EXPECTED 0", pPipeline->refcount);
	}
	json_decref(pPipeline);
}

static bool logErrorMessage(const char *errMsg, const char *pName, json_t *pStage) {
	if (errMsg) {
		char *pStageJson = json_dumps(pStage, 0);
		LOGERROR3("Pipeline::process stage:%s error:%s pStageJson:%s", pName, errMsg, pStageJson);
		free(pStageJson);
		return false;
	}
	return true;
}

void Pipeline::validateImage(Mat &image) {
	if (image.cols == 0 || image.rows == 0) {
		image = Mat(100,100, CV_8UC3);
		putText(image, "FireSight:", Point(10,20), FONT_HERSHEY_PLAIN, 1, Scalar(128,255,255));
		putText(image, "No input", Point(10,40), FONT_HERSHEY_PLAIN, 1, Scalar(128,255,255));
		putText(image, "image?", Point(10,60), FONT_HERSHEY_PLAIN, 1, Scalar(128,255,255));
	}
}

json_t *Pipeline::process(Mat &workingImage, ArgMap &argMap) { 
	Model model(argMap);
	json_t *pModelJson = model.getJson(true);

	json_t *pFireSight = json_object();
	char version[100];
	snprintf(version, sizeof(version), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	json_object_set(pFireSight, "version", json_string(version));
	json_object_set(pFireSight, "url", json_string("https://github.com/firepick1/FireSight"));
	json_object_set(pModelJson, "FireSight", pFireSight);

	model.image = workingImage;
	model.imageMap["input"] = model.image.clone();
	bool ok = processModel(model);
	workingImage = model.image;

	return pModelJson;
}

bool Pipeline::processModel(Model &model) { 
	if (!json_is_array(pPipeline)) {
		const char * errMsg = "Pipeline::process expected json array for pipeline definition";
		LOGERROR1(errMsg, "");
		throw errMsg;
	}

	bool ok = 1;
	size_t index;
	json_t *pStage;
	char debugBuf[255];
	json_array_foreach(pPipeline, index, pStage) {
		string pOp = jo_string(pStage, "op", "", model.argMap);
		string pName = jo_string(pStage, "name");
		bool isSaveImage = true;
		if (pName.empty()) {
			char defaultName[100];
			snprintf(defaultName, sizeof(defaultName), "s%d", index+1);
			pName = defaultName;
		  isSaveImage = false;
		}
		string comment = jo_string(pStage, "comment", "", model.argMap);
		json_t *pStageModel = json_object();
		json_object_set(model.getJson(false), pName.c_str(), pStageModel);
		snprintf(debugBuf,sizeof(debugBuf), "process() %s op:%s stage:%s %s", 
			matInfo(model.image).c_str(), pOp.c_str(), pName.c_str(), comment.c_str());
		if (strncmp(pOp.c_str(), "nop", 3)==0) {
			LOGTRACE1("%s (NO ACTION TAKEN)", debugBuf);
		} else if (pName.compare("input")==0) {
			ok = logErrorMessage("\"input\" is the reserved stage name for the input image", pName.c_str(), pStage);
		} else {
			LOGDEBUG1("%s", debugBuf);
			try {
			  const char *errMsg = dispatch(pOp.c_str(), pStage, pStageModel, model);
				ok = logErrorMessage(errMsg, pName.c_str(), pStage);
				if (isSaveImage) {
					model.imageMap[pName.c_str()] = model.image.clone();
				}
			} catch (runtime_error &ex) {
				ok = logErrorMessage(ex.what(), pName.c_str(), pStage);
			} catch (cv::Exception &ex) {
				ok = logErrorMessage(ex.what(), pName.c_str(), pStage);
			}
		} //if-else (pOp)
		if (!ok) { 
			LOGERROR("cancelled pipeline execution");
			ok = false;
			break; 
		}
		if (model.image.cols <=0 || model.image.rows<=0) {
			LOGERROR2("Empty working image: %dr x %dc", model.image.rows, model.image.cols);
			ok = false;
			break;
		}
	} // json_array_foreach

	LOGDEBUG2("Pipeline::processModel(stages:%d) -> %s", json_array_size(pPipeline), matInfo(model.image).c_str());

	return ok;
}

const char * Pipeline::dispatch(const char *pOp, json_t *pStage, json_t *pStageModel, Model &model) {
	bool ok = true;
  const char *errMsg = NULL;
 
	if (strcmp(pOp, "backgroundSubtractor")==0) {
		ok = apply_backgroundSubtractor(pStage, pStageModel, model);
	} else if (strcmp(pOp, "blur")==0) {
		ok = apply_blur(pStage, pStageModel, model);
	} else if (strcmp(pOp, "calcHist")==0) {
		ok = apply_calcHist(pStage, pStageModel, model);
	} else if (strcmp(pOp, "convertTo")==0) {
		ok = apply_convertTo(pStage, pStageModel, model);
	} else if (strcmp(pOp, "cout")==0) {
		ok = apply_cout(pStage, pStageModel, model);
	} else if (strcmp(pOp, "Canny")==0) {
		ok = apply_Canny(pStage, pStageModel, model);
	} else if (strcmp(pOp, "cvtColor")==0) {
		ok = apply_cvtColor(pStage, pStageModel, model);
	} else if (strcmp(pOp, "dft")==0) {
		ok = apply_dft(pStage, pStageModel, model);
	} else if (strcmp(pOp, "dftSpectrum")==0) {
		ok = apply_dftSpectrum(pStage, pStageModel, model);
	} else if (strcmp(pOp, "dilate")==0) {
		ok = apply_dilate(pStage, pStageModel, model);
	} else if (strcmp(pOp, "drawKeypoints")==0) {
		ok = apply_drawKeypoints(pStage, pStageModel, model);
	} else if (strcmp(pOp, "drawRects")==0) {
		ok = apply_drawRects(pStage, pStageModel, model);
	} else if (strcmp(pOp, "equalizeHist")==0) {
		ok = apply_equalizeHist(pStage, pStageModel, model);
	} else if (strcmp(pOp, "erode")==0) {
		ok = apply_erode(pStage, pStageModel, model);
	} else if (strcmp(pOp, "HoleRecognizer")==0) {
		ok = apply_HoleRecognizer(pStage, pStageModel, model);
	} else if (strcmp(pOp, "imread")==0) {
		ok = apply_imread(pStage, pStageModel, model);
	} else if (strcmp(pOp, "imwrite")==0) {
		ok = apply_imwrite(pStage, pStageModel, model);
	} else if (strcmp(pOp, "Mat")==0) {
		ok = apply_Mat(pStage, pStageModel, model);
	} else if (strcmp(pOp, "matchTemplate")==0) {
		ok = apply_matchTemplate(pStage, pStageModel, model);
	} else if (strcmp(pOp, "MSER")==0) {
		ok = apply_MSER(pStage, pStageModel, model);
	} else if (strcmp(pOp, "normalize")==0) {
		ok = apply_normalize(pStage, pStageModel, model);
	} else if (strcmp(pOp, "proto")==0) {
		ok = apply_proto(pStage, pStageModel, model);
	} else if (strcmp(pOp, "putText")==0) {
		ok = apply_putText(pStage, pStageModel, model);
	} else if (strcmp(pOp, "rectangle")==0) {
		ok = apply_rectangle(pStage, pStageModel, model);
	} else if (strcmp(pOp, "resize")==0) {
		ok = apply_resize(pStage, pStageModel, model);
	} else if (strcmp(pOp, "SimpleBlobDetector")==0) {
		ok = apply_SimpleBlobDetector(pStage, pStageModel, model);
	} else if (strcmp(pOp, "split")==0) {
		ok = apply_split(pStage, pStageModel, model);
	} else if (strcmp(pOp, "stageImage")==0) {
		ok = apply_stageImage(pStage, pStageModel, model);
	} else if (strcmp(pOp, "warpAffine")==0) {
		ok = apply_warpAffine(pStage, pStageModel, model);
	} else if (strcmp(pOp, "warpRing")==0) {
		ok = apply_warpRing(pStage, pStageModel, model);

	} else if (strncmp(pOp, "nop", 3)==0) {
		LOGDEBUG("Skipping nop...");
	} else {
		errMsg = "unknown op";
	}

	if (!ok) {
		errMsg = "Pipeline stage failed";
	}

	return errMsg;
}
