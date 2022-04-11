#include "../../include/raytracing/batch.hpp"

namespace kobra {

namespace rt {

////////////////////////////
// BatchIndex constructor //
////////////////////////////
	
BatchIndex::BatchIndex(int w, int h, int x, int y, int p, int l)
		: width(w), height(h), offset_x(x), offset_y(y),
		pixel_samples(p), light_samples(l) {}

////////////////////////
// BatchIndex methods //
////////////////////////

// Callback to originating batch
void BatchIndex::callback() const
{
	if (batch == nullptr)
		return;

	// Increment corresponding batch sample
	batch->increment_sample_count(*this);
}
	
// Set original batch
void BatchIndex::set_batch(Batch *batch)
{
	this->batch = batch;
}

///////////////////////
// Batch constructor //
///////////////////////

Batch::Batch(int w, int h, int bw, int bh, int maxs)
		: width(w), height(h), batch_width(bw), batch_height(bh),
		batch_x(0), batch_y(0), max_samples(maxs)
{
	// Get total number
	batches_x = (width + batch_width - 1) / batch_width;
	batches_y = (height + batch_height - 1) / batch_height;

	// Initialize sample count
	sample_count.resize(batches_x);
	for (int i = 0; i < batches_x; i++) {
		sample_count[i].resize(batches_y);
		for (int j = 0; j < batches_y; j++)
			sample_count[i][j] = 0;
	}
}

///////////////////
// Batch methods //
///////////////////
	
// Make batch index
BatchIndex Batch::make_batch_index(int x, int y, int p, int l)
{
	BatchIndex bi {
		batch_width,
		batch_height,
		x * batch_width,
		y * batch_height,
		p, l
	};

	bi.set_batch(this);
	return bi;
}

// "Increment" batch index
// TODO: should skip baches which are full
void Batch::increment(BatchIndex &index)
{
	index.offset_x += batch_width;
	if (index.offset_x >= width) {
		index.offset_x = 0;
		index.offset_y += batch_height;
	}

	if (index.offset_y >= height)
		index.offset_y = 0;
}

// Increment sample count
void Batch::increment_sample_count(const BatchIndex &index)
{
	// Get corresponding index
	int x = index.offset_x / batch_width;
	int y = index.offset_y / batch_height;

	// Increment sample count
	sample_count[x][y]++;
}

// Check if batch is fully completed
bool Batch::completed() const
{
	for (int i = 0; i < batches_x; i++) {
		for (int j = 0; j < batches_y; j++) {
			if (sample_count[i][j] < max_samples)
				return false;
		}
	}

	return true;
}

// Reset sample count
void Batch::reset()
{
	for (int i = 0; i < batches_x; i++) {
		for (int j = 0; j < batches_y; j++)
			sample_count[i][j] = 0;
	}
}

// Progress
float Batch::progress() const
{
	int total = 0;
	for (int i = 0; i < batches_x; i++) {
		for (int j = 0; j < batches_y; j++)
			total += sample_count[i][j];
	}

	return (float) total / (float) (batches_x * batches_y * max_samples);
}

}

}