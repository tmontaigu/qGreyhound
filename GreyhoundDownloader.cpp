#include <QFutureWatcher>
#include <QEventLoop>
#include <QtConcurrent>

#include <GreyhoundReader.hpp>

#include "GreyhoundDownloader.h"

std::unique_ptr<ccPointCloud> 
download_and_convert_cloud(pdal::Options opts, PDALConverter converter)
{
	std::exception_ptr eptr(nullptr);
	pdal::PointTable table;
	pdal::PointViewSet view_set;
	pdal::GreyhoundReader reader;
	std::unique_ptr<ccPointCloud> cloud(nullptr);

	reader.addOptions(opts);

	auto pdal_download = [&]() {
		try {
			reader.prepare(table);
			view_set = reader.execute(table);
			pdal::PointViewPtr view_ptr = *view_set.begin();
			cloud = converter.convert(view_ptr, table.layout());
		}
		catch (...) {
			eptr = std::current_exception();
		}
	};

	QFutureWatcher<void> downloader;
	QEventLoop loop;
	downloader.setFuture(QtConcurrent::run(pdal_download));
	QObject::connect(&downloader, &QFutureWatcher<void>::finished, &loop, &QEventLoop::quit);
	loop.exec();
	downloader.waitForFinished();

	if (eptr) {
		std::rethrow_exception(eptr);
	}
	return cloud;
}

GreyhoundDownloader::GreyhoundDownloader(pdal::Options opts, uint32_t start_depth, pdal::greyhound::Bounds bounds, PDALConverter converter)
	: m_opts(opts)
	, m_current_depth(start_depth)
	, m_bounds(bounds)
	, m_converter(converter)
{
}


void
GreyhoundDownloader::download_to(ccPointCloud *cloud, DOWNLOAD_METHOD method, std::function<void()> cb)
{
	std::queue<BoundsDepth> q;
	q.push(BoundsDepth{ m_bounds, static_cast<int>(m_current_depth) });

	while (!q.empty())
	{
		BoundsDepth m = q.front();
		pdal::Options opts(m_opts);
		opts.add("depth_begin", m.depth);
		opts.add("depth_end", m.depth + 1);
		opts.add("bounds", m.b.toJson());

		std::unique_ptr<ccPointCloud> downloaded_cloud(nullptr);
		downloaded_cloud = download_and_convert_cloud(opts, m_converter);

		if (downloaded_cloud->size() && cloud)
		{
			cloud->append(downloaded_cloud.release(), cloud->size());
			cloud->prepareDisplayForRefresh();
			cloud->refreshDisplay();
			cb();

			switch (method)
			{
			case GreyhoundDownloader::DEPTH_BY_DEPTH:
				q.push(BoundsDepth{ m.b, m.depth + 1 });
				break;
			case GreyhoundDownloader::QUADTREE:
				q.push(BoundsDepth{ m.b.getSe(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getSw(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getNe(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getNw(), m.depth + 1 });
				break;
			case GreyhoundDownloader::OCTREE:
				q.push(BoundsDepth{ m.b.getNeu(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getNed(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getNwu(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getNwd(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getSeu(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getSed(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getSwu(), m.depth + 1 });
				q.push(BoundsDepth{ m.b.getSwd(), m.depth + 1 });
				break;
			}
		}
		q.pop();
	}
}