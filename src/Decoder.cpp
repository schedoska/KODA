#include "../include/Decoder.h"
#include "../include/Utils.h"

Decoder::Decoder()
{
	root = nullptr;
}

Decoder::~Decoder()
{
}

Image Decoder::DecodeImage(std::string fileName)
{
	std::ifstream file(fileName, std::ios::binary);
	int16_t w;
	int16_t h;
	file.read((char*)&w, sizeof(int16_t));
	file.read((char*)&h, sizeof(int16_t));

	char mode;
	file.read((char*)&mode, 1);

	std::cout << "h: " << h << ", w: " << w << ", Prediction mode: " << (int)mode <<"\n";
	Image image(w, h);

	int contextSize = ModeToContextSize((ImagePrediction::Mode)mode);
	Matrix<float> params(contextSize, 1);
	for (int k = 0; k < contextSize; k++){
		float param;
		file.read((char*)&param, sizeof(float));
		params.at(k, 0) = param;
	}

	BitReader bitReader;
	root = DecodeNode(bitReader, file);

	/*std::unordered_map<int16_t, std::string> map;
	Huffman::GenerateDictionary(map, root, "");
	for (auto n : map) std::cout << n.first << " !! " << n.second << "\n";*/

	HuffmanDecoder decoder(root);
		
	bool bit;
	int16_t val;
	int count = 0;

	while(bitReader.Read(bit, file)) {
		if (decoder.PushBit(bit, val)) {
			PushToImage(image, val, count++);
		}
	}

	if (contextSize) 
		return ImagePrediction::LpcPredictionBack(ImagePrediction::LpcData(image,contextSize,params));
	return RebuildFromPrediction(image, (ImagePrediction::Mode)mode);
}

bool Decoder::PushToImage(Image& image, int16_t value, int pos)
{
	int y = pos / image.GetWidth();
	int x = pos % image.GetWidth();
	if (x >= image.GetWidth() || y >= image.GetHeight()) return false;
	image(x, y) = value;
	return true;
}

Huffman::Node* Decoder::DecodeNode(BitReader& bitReader, std::ifstream& file)
{
	bool bit;
	bitReader.Read(bit, file);

	if (bit) {
		int16_t val;
		bitReader.Read(val, file);
		return new Huffman::Node(val, 0, nullptr, nullptr, true);
	}
	else {
		Huffman::Node* leftNode = DecodeNode(bitReader, file);
		Huffman::Node* rightNode = DecodeNode(bitReader, file);
		return new Huffman::Node(0, 0, leftNode, rightNode);
	}
}

Image Decoder::RebuildFromPrediction(Image image, ImagePrediction::Mode mode)
{
	switch (mode) {
	case ImagePrediction::Mode::Left_1:
	case ImagePrediction::Mode::Left_2:
	case ImagePrediction::Mode::Left_3:
		return ImagePrediction::Left_PredictionBack(image, mode);
	case ImagePrediction::Mode::Top_1:
	case ImagePrediction::Mode::Top_2:
	case ImagePrediction::Mode::Top_3:
		return ImagePrediction::Top_PredictionBack(image, mode);
	case ImagePrediction::Mode::Median:
		return ImagePrediction::MedianPrediction(image, mode, true);
	case ImagePrediction::Mode::Graham:
		return ImagePrediction::GrahamPredictionBack(image);
	case ImagePrediction::Mode::MED_MAP:
		return ImagePrediction::MED_MAP_PredictionBack(image);
	default:
		return image;
	}
}

Decoder::HuffmanDecoder::HuffmanDecoder(Huffman::Node* root_)
{
	root = root_;
	Restart();
}

bool Decoder::HuffmanDecoder::PushBit(bool bit, int16_t& val)
{
	if (bit) current_node = current_node->right;
	else current_node = current_node->left;

	if (current_node == nullptr) {
		Restart();
		return false;
	}
	else if (current_node->isLeaf) {
		val = current_node->value;
		Restart();
		return true;
	}
	return false;
}

void Decoder::HuffmanDecoder::Restart()
{
	current_node = root;
}

Decoder::BitReader::BitReader()
{
	buffor = 0;
	count = 0;
}

bool Decoder::BitReader::Read(bool& bit, std::ifstream& file)
{
	if (count > 0) buffor <<= 1;
	else {
		if (!file.read(&buffor, 1)) return false;
		count = 8;
	}
	count--;
	bit = (buffor & 0x80);
	return true;
}

bool Decoder::BitReader::Read(int16_t& val, std::ifstream& file)
{
	bool bit;
	for (int i = 0; i < sizeof(val) * 8; i++) {
		if (Read(bit, file)) {
			val <<= 1;
			if (bit) val |= 1;
		}
		else return false;
	}
	return true;
}
